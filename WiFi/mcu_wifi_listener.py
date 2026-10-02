"""STM32F407 数字孪生 Modbus TCP 客户端。

正式链路：Python Client -> Wi-Fi/TCP 192.168.4.1:502 -> ESP8266字节网关
-> USART3 -> F407 Modbus TCP Server。旧 NMEA/$CMD 辅助函数保留作离线兼容。
"""

import socket
import struct
import threading
import time
import sys
from datetime import datetime

# ============================================================
# 配置
# ============================================================
ESP8266_IP   = "192.168.4.1"    # ESP8266 AP 模式默认 IP
ESP8266_PORT = 502               # 标准 Modbus TCP 端口
RECONNECT_INTERVAL_S = 3.0       # 掉线后重连间隔
SOCKET_TIMEOUT_S    = 3.0        # recv 超时
CMD_INTERVAL_S      = 0.5        # $CMD 控制帧发送间隔 (s)

UNIT_ID = 1
INPUT_REGISTER_COUNT = 30
POLL_INTERVAL_S = 0.2            # 5 Hz，给 ESP8266 AT 发送握手留出余量


class ModbusProtocolError(RuntimeError):
    """收到不符合 Modbus TCP 规范的响应。"""


def build_mbap_request(transaction_id: int, function: int,
                       payload: bytes, unit_id: int = UNIT_ID) -> bytes:
    pdu = bytes([function & 0xFF]) + bytes(payload)
    return struct.pack(">HHHB", transaction_id & 0xFFFF, 0,
                       1 + len(pdu), unit_id & 0xFF) + pdu


def decode_signal_registers(regs):
    if len(regs) != INPUT_REGISTER_COUNT:
        raise ModbusProtocolError(
            f"expected {INPUT_REGISTER_COUNT} registers, got {len(regs)}")
    valid = regs[2]
    return {
        "protocol_version": regs[0], "sequence": regs[1],
        "status_bitmap": regs[3],
        "jinliao": {"_valid": bool(valid & 0x01), "omega1": regs[10] / 1000.0,
                     "fish_arrived": regs[11]},
        "chuliao": {"_valid": bool(valid & 0x02), "omega": regs[12] / 1000.0,
                     "pressure_kpa": regs[13] / 100.0, "fish_drop": regs[14]},
        "jinliao02": {"_valid": bool(valid & 0x04), "omega": regs[15] / 1000.0,
                       "pressure_kpa": regs[16] / 100.0, "fish_drop": regs[17]},
        "d_hall": {"_valid": bool(valid & 0x08), "omega1": regs[18],
                   "omega2": regs[19], "omega3": regs[20], "dist1": regs[21],
                   "dist2": regs[22], "range_status": regs[23]},
        "i_hall01": {"_valid": bool(valid & 0x10), "omega1": regs[24],
                     "omega2": regs[25], "dist1": regs[26], "dist2": regs[27],
                     "dist3": regs[28], "range_status": regs[29]},
    }


class ModbusTcpClient:
    def __init__(self, host=ESP8266_IP, port=ESP8266_PORT,
                 timeout=SOCKET_TIMEOUT_S, sock=None):
        self.host, self.port, self.timeout = host, port, timeout
        self.sock = sock
        self.transaction_id = 0
        self.last_discarded_prefix = b""
        self.discarded_noise_bytes = 0

    def connect(self):
        self.close()
        self.sock = socket.create_connection((self.host, self.port), self.timeout)
        self.sock.settimeout(self.timeout)

    def close(self):
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None

    def _recv_exact(self, size):
        chunks, remaining = [], size
        while remaining:
            data = self.sock.recv(remaining)
            if not data:
                raise ConnectionError("Modbus TCP connection closed")
            chunks.append(data)
            remaining -= len(data)
        return b"".join(chunks)

    def _recv_mbap_header(self, expected_tid, scan_limit=512):
        """读取并同步到当前事务的 MBAP 头。

        某些 ESP8266 AT 固件/异常发送状态会把 ``AT+CIPSEND...`` 文本
        混入 TCP 字节流。标准响应仍以当前 transaction id + protocol id 0
        开头，因此可在有限窗口内丢弃前缀噪声并重新同步。
        """
        expected_tid_bytes = struct.pack(">H", expected_tid)
        window = bytearray()
        discarded = bytearray()
        while len(discarded) <= scan_limit:
            window.extend(self._recv_exact(1))
            if len(window) < 7:
                continue
            # A valid Modbus header with transaction 0x4154 starts
            # ``41 54 00 00``; known ESP AT noise starts ``41 54 2B``.
            if window[:3] == b"AT+":
                discarded.append(window.pop(0))
                continue
            if window[:2] == expected_tid_bytes:
                rx_tid, protocol_id, length, unit_id = struct.unpack(
                    ">HHHB", bytes(window))
                if length < 2 or length > 254:
                    raise ModbusProtocolError(f"invalid MBAP length {length}")
                if protocol_id != 0 or unit_id != UNIT_ID:
                    raise ModbusProtocolError("invalid protocol id or unit id")
                self.last_discarded_prefix = bytes(discarded)
                self.discarded_noise_bytes += len(discarded)
                return rx_tid, length
            discarded.append(window.pop(0))
        raise ModbusProtocolError("MBAP header not found within resync limit")

    def exchange(self, function, payload):
        if self.sock is None:
            raise ConnectionError("Modbus TCP socket is not connected")
        self.transaction_id = (self.transaction_id + 1) & 0xFFFF
        tid = self.transaction_id
        self.sock.sendall(build_mbap_request(tid, function, payload))
        self.last_discarded_prefix = b""
        rx_tid, length = self._recv_mbap_header(tid)
        pdu = self._recv_exact(length - 1)
        if rx_tid != tid:
            raise ModbusProtocolError(f"transaction mismatch {rx_tid} != {tid}")
        if not pdu:
            raise ModbusProtocolError("empty response PDU")
        if pdu[0] == (function | 0x80):
            code = pdu[1] if len(pdu) > 1 else -1
            raise ModbusProtocolError(f"Modbus exception {code}")
        if pdu[0] != function:
            raise ModbusProtocolError(f"function mismatch {pdu[0]:02X} != {function:02X}")
        return pdu

    def read_input_registers(self, address=0, quantity=INPUT_REGISTER_COUNT):
        pdu = self.exchange(0x04, struct.pack(">HH", address, quantity))
        if len(pdu) < 2 or pdu[1] != quantity * 2 or len(pdu) != 2 + pdu[1]:
            raise ModbusProtocolError("invalid input-register byte count")
        return list(struct.unpack(">" + "H" * quantity, pdu[2:]))

    def write_multiple_coils(self, values, address=0):
        values = [1 if v else 0 for v in values]
        if not values:
            raise ValueError("at least one coil is required")
        byte_count = (len(values) + 7) // 8
        packed = bytearray(byte_count)
        for i, value in enumerate(values):
            if value:
                packed[i // 8] |= 1 << (i % 8)
        payload = struct.pack(">HHB", address, len(values), byte_count) + packed
        pdu = self.exchange(0x0F, payload)
        if pdu != bytes([0x0F]) + struct.pack(">HH", address, len(values)):
            raise ModbusProtocolError("invalid write-multiple-coils response")

    def read_coils(self, address=0, quantity=3):
        pdu = self.exchange(0x01, struct.pack(">HH", address, quantity))
        expected_bytes = (quantity + 7) // 8
        if len(pdu) != 2 + expected_bytes or pdu[1] != expected_bytes:
            raise ModbusProtocolError("invalid coil byte count")
        return [bool((pdu[2 + i // 8] >> (i % 8)) & 1) for i in range(quantity)]

# ============================================================
# ★ 用户控制位 (可在此处修改, 每个只能为 0 或 1)
# ============================================================
FLAG1 = 1   # F407 -> PE4  (Watch 变量 g_cmd_flag1)
FLAG2 = 1   # F407 -> PE5  (Watch 变量 g_cmd_flag2)
FLAG3 = 1  # F407 -> PE6  (Watch 变量 g_cmd_flag3)
# ============================================================

# ============================================================
# NMEA 字段定义 - 必须与 F407 usart_protocol.c 中 snprintf 顺序一致
# ============================================================
# C 端: $JINLIAO,%.3f,%d,%lu*   (omega1, fish_arrived, timestamp)
JINLIAO_FIELDS = ["omega1", "fish_arrived", "timestamp"]

# C 端: $CHULIAO,%.3f,%.2f,%d,%lu* (omega, pressure_kpa, fish_drop, timestamp)
CHULIAO_FIELDS = ["omega", "pressure_kpa", "fish_drop", "timestamp"]

# ============================================================
# 解析函数
# ============================================================
def calc_nmea_checksum(body: str) -> int:
    """NMEA 风格 XOR 校验和 (与 F407 CalcXOR 一致)"""
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    return cs & 0xFF


def parse_nmea_line(line: str):
    """解析一行 NMEA 风格数据, 返回 (msg_type, fields_dict, ok)"""
    line = line.strip()
    if not line:
        return None, None, False

    if not line.startswith('$'):
        return None, None, False

    if '*' not in line:
        return None, None, False

    try:
        body, cs_str = line[1:].split('*', 1)
    except ValueError:
        return None, None, False

    try:
        cs_recv = int(cs_str[:2], 16)
    except ValueError:
        return None, None, False

    cs_calc = calc_nmea_checksum(body)
    if cs_recv != cs_calc:
        return None, None, False

    parts = body.split(',')
    msg_type = parts[0] if parts else ""

    fields = {}
    for i, v in enumerate(parts[1:], start=1):
        fields[f"f{i}"] = v

    return msg_type, fields, True


def parse_value(s: str, default=0.0):
    try:
        return float(s)
    except (ValueError, TypeError):
        return default


def parse_omega_mrad(s: str) -> int:
    """Parse the unsigned 16-bit Hall wire value and reject legacy negatives."""
    value = int(float(s))
    return max(0, min(65535, value))


def parse_frame_combined(body: str):
    """解析 v2 简化格式 $FR,<seq>,J,<o1>,<arv>,<v>;C,<o>,<prs>,<drp>,<v>;...*CS 中的 5 段子串

    F407 发送格式:
        FR,<seq>,J,o1,arv,v;C,o,prs,drp,v;P,o,prs,drp,v;D,o1,o2,o3,d1,d2,rng,v;I,o1,o2,d1,d2,d3,rng,v

    F407 字段约定:
        omega/omega1 用 *1000 整数 (rad/s)
        pressure_kpa 用 *100 整数 (cbar)
        D/I 段 omega/distance 为整数原值
    这里还原成浮点供显示.

    返回 (jinliao, jinliao02, chuliao, d_hall, i_hall01) 五元 dict
    """
    jinliao    = {"_valid": False}
    jinliao02  = {"_valid": False}
    chuliao    = {"_valid": False}
    d_hall     = {"_valid": False}
    i_hall01   = {"_valid": False}

    try:
        tokens = body.split(',')
        if len(tokens) < 3:
            return jinliao, jinliao02, chuliao, d_hall, i_hall01
        msgtype = tokens[0]
        if msgtype != "FR":
            return jinliao, jinliao02, chuliao, d_hall, i_hall01
        # tokens[1] = seq
        rest_tokens = tokens[2:]
        # 按 ';' 切分: tokens 里不会含 ';', 因为 F407 snprintf 输出 ';' 是独立字符
        blocks = []
        cur = []
        for tk in rest_tokens:
            if not tk:
                continue
            if ';' in tk:
                parts = tk.split(';')
                cur.append(parts[0])
                blocks.append(cur)
                cur = []
                for sub in parts[1:]:
                    if sub:
                        cur.append(sub)
            else:
                cur.append(tk)
        if cur:
            blocks.append(cur)

        for block in blocks:
            if not block:
                continue
            tag = block[0]
            vals = block[1:]
            try:
                if tag == "J":
                    # J:<o1_mrad>,<arv>,<v>
                    if len(vals) >= 3:
                        jinliao["omega1"]       = parse_omega_mrad(vals[0]) / 1000.0
                        jinliao["fish_arrived"] = float(vals[1])
                        jinliao["valid"]        = float(vals[2])
                        jinliao["module_id"]    = 0x01
                        jinliao["_valid"]       = (int(float(vals[2])) == 1)
                elif tag == "C":
                    # C:<o_mrad>,<prs_cbar>,<drp>,<v>
                    if len(vals) >= 4:
                        chuliao["omega"]        = parse_omega_mrad(vals[0]) / 1000.0
                        chuliao["pressure_kpa"] = float(vals[1]) / 100.0
                        chuliao["fish_drop"]    = float(vals[2])
                        chuliao["valid"]        = float(vals[3])
                        chuliao["module_id"]    = 0x05
                        chuliao["_valid"]       = (int(float(vals[3])) == 1)
                elif tag == "P":
                    # P:<o_mrad>,<prs_cbar>,<drp>,<v>
                    if len(vals) >= 4:
                        jinliao02["omega"]        = parse_omega_mrad(vals[0]) / 1000.0
                        jinliao02["pressure_kpa"] = float(vals[1]) / 100.0
                        jinliao02["fish_drop"]    = float(vals[2])
                        jinliao02["valid"]        = float(vals[3])
                        jinliao02["module_id"]    = 0x02
                        jinliao02["_valid"]       = (int(float(vals[3])) == 1)
                elif tag == "D":
                    # D:<o1>,<o2>,<o3>,<d1>,<d2>,<rng>,<v>
                    if len(vals) >= 7:
                        d_hall["omega1"]       = parse_omega_mrad(vals[0])
                        d_hall["omega2"]       = parse_omega_mrad(vals[1])
                        d_hall["omega3"]       = parse_omega_mrad(vals[2])
                        d_hall["dist1"]        = int(float(vals[3]))
                        d_hall["dist2"]        = int(float(vals[4]))
                        d_hall["range_status"] = int(float(vals[5]))
                        d_hall["valid"]        = int(float(vals[6]))
                        d_hall["module_id"]    = 0x03
                        d_hall["_valid"]       = (int(float(vals[6])) == 1)
                elif tag == "I":
                    # I:<o1>,<o2>,<d1>,<d2>,<d3>,<rng>,<v>
                    if len(vals) >= 7:
                        i_hall01["omega1"]       = parse_omega_mrad(vals[0])
                        i_hall01["omega2"]       = parse_omega_mrad(vals[1])
                        i_hall01["dist1"]        = int(float(vals[2]))
                        i_hall01["dist2"]        = int(float(vals[3]))
                        i_hall01["dist3"]        = int(float(vals[4]))
                        i_hall01["range_status"] = int(float(vals[5]))
                        i_hall01["valid"]        = int(float(vals[6]))
                        i_hall01["module_id"]    = 0x04
                        i_hall01["_valid"]       = (int(float(vals[6])) == 1)
            except (ValueError, IndexError):
                continue
    except Exception:
        pass
    return jinliao, jinliao02, chuliao, d_hall, i_hall01


# ============================================================
# 帧切分 (抗粘包 + 抗 AT 噪声)
# ============================================================
def extract_nmea_frames(rx_buf: bytes):
    """
    从字节流里切分出所有以 $ 开头、以 \\n 结尾的完整 NMEA 行.

    抗 AT 命令回显 (例如 "AT+CIPSEND=0,71\\r\\n$FR,..."):
      - 用 \\n 切分, 然后每行从右向左找第一个 '$' (NMEA 帧头),
        丢弃 '$' 之前的所有字节 (可能是 "AT+CIPSEND=0,71\\r" 之类回显)
      - 没 \\n 的不完整帧留到下次

    @param rx_buf: bytes 原始接收缓冲区
    @return: (list[str] 完整行, bytes 剩余 buffer)
    @return truncated: int (0=无残缺, 1=有残缺帧被丢弃)
    """
    # 找最后一个 \n, 切分完整行 vs 不完整尾巴
    last_nl = rx_buf.rfind(b'\n')
    if last_nl < 0:
        # 整个 buffer 都没有 \n, 全部留到下次
        return [], rx_buf, 0

    complete = rx_buf[:last_nl + 1]   # 含最后一个 \n
    pending = rx_buf[last_nl + 1:]    # 不完整尾巴, 下次拼接

    # 按 \n 切分, 每行反向找 $, 丢弃之前内容
    raw_lines = complete.split(b'\n')
    lines = []
    for raw in raw_lines:
        # 跳过空行
        if not raw:
            continue
        # 从右向左找 '$'
        dollar_idx = raw.rfind(b'$')
        if dollar_idx < 0:
            continue  # 整行没有 $, 全是噪声, 丢弃
        line_bytes = raw[dollar_idx:]
        line = line_bytes.decode('utf-8', errors='replace').strip()
        if line:
            lines.append(line)

    truncated = 0 if not pending else 1
    return lines, pending, truncated


# ============================================================
# 统计
# ============================================================
class Stats:
    def __init__(self):
        self.total = 0                # 累计接收包数
        self.ok = 0                   # 成功解析包数
        self.bad_cs = 0               # 校验和失败
        self.unknown = 0              # 格式未知
        self.jinliao = 0              # J 段 (0x01 jinliao01) 有效帧
        self.jinliao02 = 0            # P 段 (0x02 jinliao02) 有效帧
        self.chuliao = 0              # C 段 (0x05 chuliao05) 有效帧
        self.d_hall = 0               # D 段 (0x03 F407HALL) 有效帧
        self.i_hall01 = 0             # I 段 (0x04 F407HALL01) 有效帧
        self.reconnects = 0
        self.rx_bytes_total = 0       # 累计接收字节
        self.rx_truncated_lines = 0   # 残缺帧次数
        # F407 暂未填 timestamp, 丢包估算不可用


def fmt_float(v: float) -> str:
    if abs(v) < 1e-6:
        return f"{v:.6f}"
    if abs(v) > 1e6 or abs(v) < 1e-3:
        return f"{v:.4e}"
    return f"{v:>12.4f}"


def connect_with_retry():
    """持续尝试连接 ESP8266, 直到成功"""
    attempt = 0
    while True:
        attempt += 1
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(SOCKET_TIMEOUT_S)
            print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                  f"尝试连接 {ESP8266_IP}:{ESP8266_PORT} ... (第 {attempt} 次)")
            sock.connect((ESP8266_IP, ESP8266_PORT))
            print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                  f"✓ TCP 连接成功")
            return sock
        except (ConnectionRefusedError, OSError, socket.timeout) as e:
            ts = datetime.now().strftime('%H:%M:%S')
            print(f"[{ts}] ✗ 连接失败: {e}")
            if attempt == 1:
                print("    可能原因:")
                print("    1. PC 未连接到 ESP8266 热点 (BinghuoLink / 密码: wildfire123)")
                print("    2. F407 未上电, ESP8266 未工作")
                print("    3. Windows 防火墙拦截:")
                print("       netsh advfirewall set allprofiles state off")
                print("    4. ESP8266 客户端尚未连接 (AP 模式下 PC 主动连 ESP8266)")
            print(f"    {RECONNECT_INTERVAL_S:.0f} 秒后重试... (Ctrl+C 退出)")
            try:
                sock.close()
            except Exception:
                pass
            time.sleep(RECONNECT_INTERVAL_S)


# ============================================================
# ★ 控制帧发送线程 (向 F407 周期性发 $CMD,f1,f2,f3*CS)
# ============================================================
def build_cmd_packet(f1: int, f2: int, f3: int) -> bytes:
    """构建 $CMD,<f1>,<f2>,<f3>*CS\\r\\n (XOR 校验范围: $ 和 * 之间)"""
    body = f"CMD,{int(f1) & 1},{int(f2) & 1},{int(f3) & 1}"
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    return f"${body}*{cs & 0xFF:02X}\r\n".encode("ascii")


class CmdSender:
    """独立线程, 周期性向 F407 发送 $CMD 控制帧.
       主线程 recv 时不会因为 send 而阻塞 (避免 PC 端 recv 缓冲区填满导致 ESP8266 关闭)."""

    def __init__(self, sock_getter, interval_s=CMD_INTERVAL_S):
        self._sock_getter = sock_getter  # callable returning current socket (or None)
        self._interval_s = interval_s
        self._stop_evt   = threading.Event()
        self._thread     = None
        self.tx_count    = 0
        self.last_packet = b""

    def start(self):
        self._thread = threading.Thread(
            target=self._run, name="CmdSender", daemon=True)
        self._thread.start()

    def stop(self):
        self._stop_evt.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)

    def _run(self):
        while not self._stop_evt.is_set():
            sock = self._sock_getter()
            if sock is None:
                time.sleep(self._interval_s)
                continue
            try:
                pkt = build_cmd_packet(FLAG1, FLAG2, FLAG3)
                sock.sendall(pkt)
                self.tx_count += 1
                self.last_packet = pkt
                ts = datetime.now().strftime('%H:%M:%S')
                print(f"[{ts}] [CMD TX #{self.tx_count}] {pkt.decode('ascii', errors='replace').rstrip()}", flush=True)
            except (OSError, BrokenPipeError) as e:
                # send 失败一般意味着连接已断开, 主线程 recv 会感知, 只需稍等
                ts = datetime.now().strftime('%H:%M:%S')
                print(f"[{ts}] [CMD TX] send 失败: {e}", flush=True)
                time.sleep(self._interval_s)
                continue
            time.sleep(self._interval_s)


# ============================================================
# 主程序
# ============================================================
def legacy_main():
    print("=" * 70)
    print("  STM32F407 数字孪生 WiFi (ESP8266 AP 透传) 监听器")
    print(f"  连接: {ESP8266_IP}:{ESP8266_PORT} (TCP)")
    print("  协议: NMEA 风格 $FRAME 合并包 (5 路: J:0x01, P:0x02, C:0x05, D:0x03, I:0x04)")
    print("  请先确认 PC 已连接 WiFi 热点 BinghuoLink (密码: wildfire123)")
    print("  ★ 控制位: FLAG1=%d -> PE4, FLAG2=%d -> PE5, FLAG3=%d -> PE6"
          % (FLAG1, FLAG2, FLAG3))
    print(f"  ★ $CMD 帧发送间隔: {CMD_INTERVAL_S:.2f}s")
    print("  按 Ctrl+C 退出")
    print("=" * 70)

    stats = Stats()
    sock = None
    rx_buf = b""   # TCP 字节流缓存, 按 \n 切分

    # ★ 用 dict 让 CmdSender 线程始终读到最新的 socket (重连后也能跟上)
    sock_holder = {"sock": None}
    sender = CmdSender(lambda: sock_holder["sock"], interval_s=CMD_INTERVAL_S)
    sender.start()

    try:
        while True:
            if sock is None:
                sock = connect_with_retry()
                sock_holder["sock"] = sock
                stats.reconnects += 1
                rx_buf = b""
                start = time.time()
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] "
                      f"等待 F407 发包...\n")

            try:
                data = sock.recv(4096)
            except socket.timeout:
                elapsed = int(time.time() - start)
                if elapsed >= 5:
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                          f"还在等... ({elapsed}秒, 已收 {stats.total} 包, "
                          f"jinliao={stats.jinliao}, chuliao={stats.chuliao})")
                    start = time.time()
                continue

            if not data:
                print(f"\n[{datetime.now().strftime('%H:%M:%S')}] "
                      f"✗ TCP 连接被断开")
                sock.close()
                sock = None
                sock_holder["sock"] = None
                continue

            # 累积到 rx_buf, 用 $ 切分出完整 NMEA 行 (抗粘包 + 抗 AT 噪声)
            stats.rx_bytes_total += len(data)
            rx_buf += data
            lines, rx_buf, truncated = extract_nmea_frames(rx_buf)
            stats.rx_truncated_lines += truncated

            # ★ DEBUG sniff: 如果有原始字节但没切出 NMEA 行, 把原始数据 hex 出来
            # 这能区分"F407 完全没发" vs "F407 发了但格式不对"
            if data and not lines:
                hex_preview = data[:120].hex(' ')
                print(f"[{datetime.now().strftime('%H:%M:%S')}] "
                      f"⚠ 收到 {len(data)}B 但不是 NMEA 行: {hex_preview}{'...' if len(data) > 120 else ''}")

            for line in lines:
                handle_line(line, stats)

    except KeyboardInterrupt:
        pass
    finally:
        sender.stop()
        sock_holder["sock"] = None
        if sock:
            try:
                sock.close()
            except Exception:
                pass
        print()
        print("=" * 70)
        print("  退出统计:")
        print(f"    TCP 重连次数:   {stats.reconnects}")
        print(f"    总接收包数:    {stats.total}")
        print(f"    累计接收字节:   {stats.rx_bytes_total}")
        print(f"    残缺帧次数:     {stats.rx_truncated_lines}")
        print(f"    校验和失败:    {stats.bad_cs}")
        print(f"    未知格式:      {stats.unknown}")
        print(f"    $FRAME 总数:   {stats.ok}")
        print(f"    J 段 (0x01):   {stats.jinliao}")
        print(f"    P 段 (0x02):   {stats.jinliao02}")
        print(f"    C 段 (0x05):   {stats.chuliao}")
        print(f"    D 段 (0x03):   {stats.d_hall}")
        print(f"    I 段 (0x04):   {stats.i_hall01}")
        print(f"    $CMD 发送次数: {sender.tx_count}")
        if sender.last_packet:
            print(f"    最后发送:      {sender.last_packet.decode('ascii', errors='replace').rstrip()}")
        print("=" * 70)


def handle_line(line: str, stats: Stats):
    """解析并打印一行 NMEA 数据"""
    stats.total += 1
    ts = datetime.now().strftime('%H:%M:%S.') + \
         f"{int(datetime.now().microsecond / 1000):03d}"

    msg_type, fields, ok = parse_nmea_line(line)

    if not ok:
        if line.startswith('$') and '*' in line:
            try:
                body = line[1:].split('*', 1)[0]
                cs_recv = int(line.split('*')[1][:2], 16)
                cs_calc = calc_nmea_checksum(body)
                if cs_recv != cs_calc:
                    stats.bad_cs += 1
                    print(f"[{ts}] #{stats.total} 校验和错 "
                          f"(收到={cs_recv:02X}, 计算={cs_calc:02X})")
                    print(f"  RAW: {line}")
                    return
            except Exception:
                pass
        stats.unknown += 1
        print(f"[{ts}] #{stats.total} 格式未知: {line}")
        return

    stats.ok += 1

    if msg_type == "JINLIAO" or msg_type == "CHULIAO" or msg_type == "HALL":
        # 旧版单条消息格式已在 C 端废弃, 统一合并到 $FRAME
        stats.unknown += 1
        print(f"[{ts}] #{stats.total} [{msg_type}] 旧版单条消息已废弃, F407 仅发 $FRAME 合并包: {line}")
        return

    elif msg_type == "FR":
        # v2 简化格式: 解析 J/C/P/D/I 5 段子串
        body_for_sub = line[1:].split('*', 1)[0]
        jinliao, jinliao02, chuliao, d_hall, i_hall01 = parse_frame_combined(body_for_sub)

        if jinliao.get("_valid"):
            stats.jinliao += 1
        if jinliao02.get("_valid"):
            stats.jinliao02 += 1
        if chuliao.get("_valid"):
            stats.chuliao += 1
        if d_hall.get("_valid"):
            stats.d_hall += 1
        if i_hall01.get("_valid"):
            stats.i_hall01 += 1

        print(f"[{ts}] #{stats.total} [FRAME] (5 路传感器合并包: J/C/P/D/I)")
        # J 段 — F103jinliao01 (0x01)
        if jinliao.get("_valid"):
            print(f"    [J:0x01] omega1       = {fmt_float(jinliao.get('omega1', 0))} rad/s   (进料带1)")
            fa = int(jinliao.get('fish_arrived', 0))
            print(f"    [J:0x01] fish_arrived = {fa}"
                  f"{'                  (有鱼)' if fa else '                  (无鱼)'}")
        else:
            print(f"    [J:0x01] (无 0x01 进料1 数据)")
        # P 段 — F103jinliao02 (0x02)
        if jinliao02.get("_valid"):
            print(f"    [P:0x02] omega        = {fmt_float(jinliao02.get('omega', 0))} rad/s   (进料带2 霍尔)")
            print(f"    [P:0x02] pressure_kpa = {fmt_float(jinliao02.get('pressure_kpa', 0))} kPa")
            fd = int(jinliao02.get('fish_drop', 0))
            print(f"    [P:0x02] fish_drop    = {fd}"
                  f"{'                  (有鱼落下)' if fd else '                  (无鱼落下)'}")
        else:
            print(f"    [P:0x02] (无 0x02 进料2 数据)")
        # C 段 — F103chuliao05 (0x05)
        if chuliao.get("_valid"):
            print(f"    [C:0x05] omega        = {fmt_float(chuliao.get('omega', 0))} rad/s   (出料带)")
            print(f"    [C:0x05] pressure_kpa = {fmt_float(chuliao.get('pressure_kpa', 0))} kPa")
            fd = int(chuliao.get('fish_drop', 0))
            print(f"    [C:0x05] fish_drop    = {fd}"
                  f"{'                  (有鱼落下)' if fd else '                  (无鱼落下)'}")
        else:
            print(f"    [C:0x05] (无 0x05 出料数据)")
        # D 段 — F407HALL (0x03)
        if d_hall.get("_valid"):
            om1 = int(d_hall.get("omega1", 0))
            om2 = int(d_hall.get("omega2", 0))
            om3 = int(d_hall.get("omega3", 0))
            d1  = int(d_hall.get("dist1", 0))
            d2  = int(d_hall.get("dist2", 0))
            rs  = int(d_hall.get("range_status", 0))
            print(f"    [D:0x03] omega1={om1} omega2={om2} omega3={om3} "
                  f"dist1={d1}mm dist2={d2}mm range_status=0x{rs:02X}")
        else:
            print(f"    [D:0x03] (无 0x03 F407HALL 数据)")
        # I 段 — F407HALL01 I2C3 (0x04)
        if i_hall01.get("_valid"):
            om1 = int(i_hall01.get("omega1", 0))
            om2 = int(i_hall01.get("omega2", 0))
            d1  = int(i_hall01.get("dist1", 0))
            d2  = int(i_hall01.get("dist2", 0))
            d3  = int(i_hall01.get("dist3", 0))
            rs  = int(i_hall01.get("range_status", 0))
            print(f"    [I:0x04] omega1={om1} omega2={om2} "
                  f"dist1={d1}mm dist2={d2}mm dist3={d3}mm range_status=0x{rs:03X}")
        else:
            print(f"    [I:0x04] (无 0x04 F407HALL01 数据)")
        print(f"    累计: jinliao={stats.jinliao}, jinliao02={stats.jinliao02}, "
              f"chuliao={stats.chuliao}, d_hall={stats.d_hall}, i_hall01={stats.i_hall01}")
        print()

    else:
        stats.unknown += 1
        print(f"[{ts}] #{stats.total} [{msg_type}] 未知消息类型: {line}")
        print()


def _print_modbus_snapshot(data, gap_count):
    ts = datetime.now().strftime('%H:%M:%S.%f')[:-3]
    j, c, p = data["jinliao"], data["chuliao"], data["jinliao02"]
    d, i = data["d_hall"], data["i_hall01"]
    print(f"[{ts}] Modbus seq={data['sequence']} gaps={gap_count} "
          f"valid=J{int(j['_valid'])} C{int(c['_valid'])} P{int(p['_valid'])} "
          f"D{int(d['_valid'])} I{int(i['_valid'])}")
    if j["_valid"]:
        print(f"  J omega1={j['omega1']:.3f}rad/s arrived={j['fish_arrived']}")
    if c["_valid"]:
        print(f"  C omega={c['omega']:.3f}rad/s pressure={c['pressure_kpa']:.2f}kPa drop={c['fish_drop']}")
    if p["_valid"]:
        print(f"  P omega={p['omega']:.3f}rad/s pressure={p['pressure_kpa']:.2f}kPa drop={p['fish_drop']}")
    if d["_valid"]:
        print(f"  D omega={d['omega1']}/{d['omega2']}/{d['omega3']}mrad/s dist={d['dist1']}/{d['dist2']}mm status=0x{d['range_status']:X}")
    if i["_valid"]:
        print(f"  I omega={i['omega1']}/{i['omega2']}mrad/s dist={i['dist1']}/{i['dist2']}/{i['dist3']}mm status=0x{i['range_status']:X}")


def _print_resync_warning(client):
    if not client.last_discarded_prefix:
        return
    raw = client.last_discarded_prefix
    preview = raw[:80].decode("ascii", errors="replace").replace("\r", "\\r").replace("\n", "\\n")
    print(f"警告: Modbus流前缀含 {len(raw)} 字节非MBAP数据，已重新同步: {preview}")


def main():
    print("STM32F407 数字孪生 Modbus TCP 主机")
    print(f"连接 {ESP8266_IP}:{ESP8266_PORT}, Unit ID={UNIT_ID}, 输入寄存器 0..29")
    print(f"线圈 0..2 -> PE4/PE5/PE6: {FLAG1}/{FLAG2}/{FLAG3}")
    client = ModbusTcpClient()
    last_sequence, sequence_gaps, last_coil_refresh = None, 0, 0.0
    while True:
        try:
            if client.sock is None:
                print(f"[{datetime.now().strftime('%H:%M:%S')}] 正在连接 Modbus TCP...")
                client.connect()
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Modbus TCP 已连接")
                last_sequence, last_coil_refresh = None, 0.0
            start = time.monotonic()
            data = decode_signal_registers(client.read_input_registers())
            _print_resync_warning(client)
            sequence = data["sequence"]
            if last_sequence is not None:
                delta = (sequence - last_sequence) & 0xFFFF
                if delta > 1:
                    sequence_gaps += delta - 1
            last_sequence = sequence
            now = time.monotonic()
            if now - last_coil_refresh >= CMD_INTERVAL_S:
                expected = [bool(FLAG1), bool(FLAG2), bool(FLAG3)]
                client.write_multiple_coils(expected)
                _print_resync_warning(client)
                actual = client.read_coils()
                _print_resync_warning(client)
                if actual != expected:
                    print(f"警告: 线圈回读不一致 expected={expected} actual={actual}")
                last_coil_refresh = now
            _print_modbus_snapshot(data, sequence_gaps)
            delay = POLL_INTERVAL_S - (time.monotonic() - start)
            if delay > 0:
                time.sleep(delay)
        except (OSError, ConnectionError, ModbusProtocolError) as exc:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] 通信异常: {exc}; {RECONNECT_INTERVAL_S:.0f}s 后重连")
            client.close()
            time.sleep(RECONNECT_INTERVAL_S)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
