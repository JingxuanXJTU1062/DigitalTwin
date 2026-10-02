# F407—Python Modbus TCP 数字孪生通信设计

## 目标

将数字孪生主机与作品原型之间的正式链路改为标准 Modbus TCP，同时保留现有 J/C/P/D/I 业务字段、数值单位和缩放规则。ESP8266 继续作为 Wi-Fi/TCP 到 USART3 的字节网关，不承担 Modbus 业务解析。

验收时应能明确证明：

- Python 数字孪生端是 Modbus TCP Client。
- F407 物理端网关是 Modbus TCP Server。
- 传感器通过输入寄存器映射，控制输出通过线圈映射。
- MBAP、功能码、异常响应、事务号和地址范围符合 Modbus TCP 基本规则。
- 断线、坏帧和陈旧数据不会被继续标记为有效或保持危险输出。

## 架构

```text
物理传感器/执行器
  │ UART/I2C（现有 AA55 + XOR 采集协议）
  ▼
STM32F407（Modbus TCP Server，Unit ID 1）
  │ USART3 115200 8N1，ESP8266 AT + CIPSEND/+IPD
  ▼
ESP8266（AP + TCP Server，仅转发字节）
  │ Wi-Fi 2.4 GHz，TCP 192.168.4.1:502
  ▼
Python 主机（Modbus TCP Client，数字孪生信号映射）
```

ESP8266 启动时明确设置 `AT+CIPDINFO=0`，下行格式固定为 `+IPD,<link_id>,<length>:<binary payload>`。F407 按 `length` 读取二进制数据，再按 MBAP 的 Length 字段重组完整 ADU；不得再依赖 `$`、换行或 C 字符串结束符。

## 协议角色与网络参数

- Wi-Fi SSID、密码和 AP 模式保持现有配置。
- TCP监听地址：`192.168.4.1:502`。
- F407：Modbus TCP Server。
- Python：Modbus TCP Client。
- Modbus Unit ID：`1`。
- Python 每 100 ms读取输入寄存器；每 500 ms刷新三个控制线圈。
- TCP断开后 Python 自动重连，F407继续扫描 ESP8266连接状态。

## 输入寄存器映射（功能码 0x04）

下表地址均为 Modbus PDU 中的零基地址；若文档采用 3xxxx 表示法，则地址 0 对应 30001。

| 地址 | 信号 | 数据类型 | 单位/缩放 |
|---:|---|---|---|
| 0 | protocol_version | uint16 | `0x0100` |
| 1 | sequence | uint16 | 每次快照递增 |
| 2 | valid_bitmap | uint16 | bit0=J、bit1=C、bit2=P、bit3=D、bit4=I |
| 3 | status_bitmap | uint16 | bit0=Modbus运行、bit1=命令新鲜 |
| 4～9 | reserved | uint16 | 固定 0 |
| 10 | J.omega1 | uint16 | rad/s × 1000 |
| 11 | J.fish_arrived | uint16 | 0/1 |
| 12 | C.omega | uint16 | rad/s × 1000 |
| 13 | C.pressure_kpa | uint16 | kPa × 100 |
| 14 | C.fish_drop | uint16 | 0/1 |
| 15 | P.omega | uint16 | rad/s × 1000 |
| 16 | P.pressure_kpa | uint16 | kPa × 100 |
| 17 | P.fish_drop | uint16 | 0/1 |
| 18 | D.omega1 | uint16 | 原始 mrad/s |
| 19 | D.omega2 | uint16 | 原始 mrad/s |
| 20 | D.omega3 | uint16 | 原始 mrad/s |
| 21 | D.dist1 | uint16 | mm |
| 22 | D.dist2 | uint16 | mm |
| 23 | D.range_status | uint16 | 原状态值 |
| 24 | I.omega1 | uint16 | 原始 mrad/s |
| 25 | I.omega2 | uint16 | 原始 mrad/s |
| 26 | I.dist1 | uint16 | mm |
| 27 | I.dist2 | uint16 | mm |
| 28 | I.dist3 | uint16 | mm |
| 29 | I.range_status | uint16 | 原状态值 |

Python 仍将速度恢复为 rad/s、压力恢复为 kPa，并按原先 J/C/P/D/I 名称展示，因此业务数据格式和数字孪生字段语义不变。

## 线圈映射

| 线圈地址 | F407输出 | 业务含义 |
|---:|---|---|
| 0 | PE4 | 原 FLAG1 |
| 1 | PE5 | 原 FLAG2 |
| 2 | PE6 | 原 FLAG3 |

支持：

- `0x01` Read Coils。
- `0x05` Write Single Coil。
- `0x0F` Write Multiple Coils。

Python 每 500 ms使用 `0x0F` 刷新三个线圈，并通过 `0x01` 回读确认。F407 若 1500 ms未收到合法线圈写请求，必须将 PE4/PE5/PE6 全部恢复为低电平。

## Modbus TCP处理规则

F407验证：

- MBAP Transaction ID原样回显。
- Protocol ID必须为 0。
- Length必须与实际 ADU长度一致。
- Unit ID必须为 1；其他 Unit ID不处理。
- 输入寄存器读取范围必须落在 0～29。
- 线圈访问范围必须落在 0～2。
- 单次请求长度受本地缓冲区上限约束。

异常响应：

- 不支持的功能码：`0x01 Illegal Function`。
- 非法地址或跨界范围：`0x02 Illegal Data Address`。
- 非法数量、字节数或线圈写值：`0x03 Illegal Data Value`。

每个 ESP8266 link ID有独立接收缓存，允许 TCP拆包、粘包和不同客户端数据交错。响应必须发回产生请求的同一 link ID。

## 可靠性与安全状态

- J/C/P/D 四路 UART数据保存最后接收时间，超过 500 ms未收到合法帧则清除对应有效位；I 路沿用 500 ms超时。
- 错误帧不得刷新时间戳。
- ESP8266发送必须收到 `>` 提示后才发送二进制载荷；超时返回失败，不得假报成功。
- 二进制载荷使用显式长度的 UART发送接口，禁止 `%s`。
- 修复 `g_link_mask == 0` 时跳过后续链路扫描的问题。
- USART3共享接收缓冲区在使用字符串搜索前必须按当前长度补 NUL，且不能用字符串函数处理 Modbus二进制载荷。
- Python核对 Transaction ID、Protocol ID、Unit ID、功能码及响应长度；异常响应和超时均触发明确日志，网络错误触发重连。

## 兼容性

- `Protocol_BuildCombinedNMEA()`、旧 `$FR`解析辅助函数和 `$CMD`构造辅助函数保留，供离线调试和历史数据兼容。
- 正式 `main` 运行路径不再主动向 Modbus连接发送 `$FR`，避免破坏请求—响应语义。
- 原 J/C/P/D/I 字段、单位、整数缩放和三个控制位不变。
- README 更新为 Modbus TCP正式链路，并将旧 NMEA 标为“遗留调试格式”。

## 文件边界

- 新建 `User/Modbus/modbus_tcp_server.c/.h`：纯 Modbus ADU解析、寄存器快照和线圈处理。
- 新建 `User/ESP8266/esp8266_ipd.c/.h`：`+IPD`头与按长度二进制载荷解析、每 link 重组。
- 修改 `bsp_esp8266.c/.h`：可靠的二进制 `CIPSEND`。
- 修改 `bsp_debug_usart.c`：USART3字节送入 IPD解析器，并保证 AT缓冲区边界安全。
- 修改 `main.c`：处理 Modbus请求、数据过期和命令安全超时，不再周期推送 NMEA。
- 修改 `usart_protocol.c/.h`：统一 UART通道时间戳与过期接口。
- 修改 `bsp_esp8266_test.c/.h`：端口 502、`CIPDINFO=0`。
- 修改 Keil工程：加入新 C文件。
- 重写 `mcu_wifi_listener.py` 正式运行路径为标准库实现的 Modbus TCP Client，同时保留遗留解析辅助函数。
- 新建自动化测试，验证 Python协议编解码、C端 Modbus解析和寄存器映射。

## 测试与验收

自动化测试至少覆盖：

- 正常读取 0～29 输入寄存器。
- 三种线圈功能码和线圈回读。
- 非法功能码、地址、数量、字节数和写值的异常响应。
- MBAP长度、事务号、协议号和 Unit ID验证。
- `+IPD`头拆分、载荷拆分、多个 ADU粘包和多 link交错。
- 二进制报文包含 `0x00`。
- 五路数据的原缩放规则与 Python显示值。
- UART 500 ms陈旧数据失效。
- 控制 1500 ms超时回低电平。
- TCP断线重连后恢复轮询。

最终验证包括 Python完整测试、Python语法检查、桌面可运行的 C协议单元测试，以及 Keil F407完整重新构建。没有实物连接时，明确区分“软件验证通过”和“尚待硬件联调”。
