/**
  ******************************************************************************
  * @file    usart_protocol.c
  * @brief   UART protocol implementation (F407_04 HAL version)
  *
  *  Receive pipeline (与 F407_03 工程保持一致):
  *    1. main 循环检测 s_rx_flag -> 调用 Protocol_UARTx_RxCpltCallback()
  *         -> push 1 字节到 ring buffer
  *         (与 F407_03 HAL_UART_RxCpltCallback 等价)
  *    2. main 循环调用 Protocol_ProcessBuffer()
  *         -> pop 1 字节, 跑 AA55 状态机
  *    3. 状态机收完一帧 -> XOR 校验 -> ParseJinliao/ParseChuliaoFrame
  *
  *  Frame format (must match F103 sender code):
  *    jinliao (10B): AA 55 01 05 omega1_LE fish_arr XOR
  *    chuliao (12B): AA 55 05 07 omega_LE pressure_LE fish_drop 0x00 XOR
  ******************************************************************************
  */
#include "usart_protocol.h"
#include <string.h>
#include <stdio.h>

/* ================== Forward declarations (for ring helpers) ================== */
static void     RxRing_Push(uint8_t idx, uint8_t byte);
static uint8_t  RxRing_Pop(uint8_t idx);
static uint16_t RxRing_Available(uint8_t idx);

/* ================== 全局数据 (Watch 窗口观察) ================== */
volatile JinliaoData_TypeDef g_jinliao_data = {0};
volatile ChuliaoData_TypeDef g_chuliao_data = {0};
volatile Jinliao02Data_TypeDef g_jinliao02_data = {0};
volatile Hall01I2CData_TypeDef g_hall01_i2c_data = {0};

volatile uint8_t  g_rx_last_frame_jinliao[10] = {0};
volatile uint8_t  g_rx_last_frame_chuliao[12] = {0};
volatile uint8_t  g_rx_last_frame_jinliao02[12]     = {0};  /* F103jinliao02 (0x02) */
volatile uint8_t  g_rx_last_frame_hall_vl6180x[FRAME_HALL_VL6180X_LEN] = {0}; /* F407HALL (0x03) */
volatile uint8_t  g_rx_frame_valid_jinliao = 0;
volatile uint8_t  g_rx_frame_valid_chuliao = 0;
volatile uint8_t  g_rx_frame_valid_jinliao02      = 0;
volatile uint8_t  g_rx_frame_valid_hall_vl6180x   = 0;
static volatile uint32_t s_hall_vl6180x_timestamp = 0;
volatile uint8_t  g_rx_uart_idx_jinliao = 0;
volatile uint8_t  g_rx_uart_idx_chuliao = 0;
volatile uint8_t  g_rx_uart_idx_jinliao02      = 0;
volatile uint8_t  g_rx_uart_idx_hall_vl6180x   = 0;

volatile RxRing_TypeDef g_rx_ring[UART_COUNT] = {0};

volatile uint32_t g_rx_count_jinliao = 0;
volatile uint32_t g_rx_count_chuliao = 0;
volatile uint32_t g_rx_count_jinliao02     = 0;
volatile uint32_t g_rx_count_hall_vl6180x  = 0;
volatile uint8_t  g_debug_last_byte    = 0;
volatile uint8_t  g_debug_parse_state  = 0;
volatile uint8_t  g_debug_module_id    = 0;
volatile uint8_t  g_debug_frame_len    = 0;

/* ================== 内部 ISR/COMM buffer (no longer used, kept empty for ABI compat) ================== */
static volatile uint8_t s_rx_byte[UART_COUNT];
static volatile uint8_t s_rx_flag[UART_COUNT];

/* ================== 状态机 ================== */
/* UART 帧最长 17B；USART2 接收 F407HALL03 的 0x04 帧。 */
static uint8_t s_frame_buf[UART_COUNT][32] = {0};
static uint8_t s_frame_len[UART_COUNT]     = {0};
static uint8_t s_parse_state[UART_COUNT]   = {0};
static uint8_t s_expected_len[UART_COUNT]  = {0};

/* ================== Ring buffer ================== */
static void RxRing_Push(uint8_t idx, uint8_t byte)
{
    uint16_t next = (g_rx_ring[idx].head + 1U) % UART_RX_RING_SIZE;
    if (next != g_rx_ring[idx].tail) {
        g_rx_ring[idx].data[g_rx_ring[idx].head] = byte;
        g_rx_ring[idx].head = next;
    }
}

static uint8_t RxRing_Pop(uint8_t idx)
{
    uint8_t byte = g_rx_ring[idx].data[g_rx_ring[idx].tail];
    g_rx_ring[idx].tail = (g_rx_ring[idx].tail + 1U) % UART_RX_RING_SIZE;
    return byte;
}

static uint16_t RxRing_Available(uint8_t idx)
{
    return (uint16_t)(g_rx_ring[idx].head + UART_RX_RING_SIZE - g_rx_ring[idx].tail)
           % UART_RX_RING_SIZE;
}

/* ================== ISR-side: directly push into ring buffer ================
 * Called from USART1_IRQHandler / UART4_IRQHandler in bsp_debug_usart.c
 * ISR 直接 push 到 ring, 避免 s_rx_byte + s_rx_flag 单中转丢字节
 * =========================================================================== */
void Protocol_ParseByte(uint8_t uart_idx, uint8_t byte)
{
    if (uart_idx >= UART_COUNT) return;
    g_debug_last_byte = byte;

    RxRing_Push(uart_idx, byte);
}

/* ================== 兼容旧 main 调用: 什么都不做 (ISR 已 push) =============== */
void Protocol_UART4_RxCpltCallback(void)
{
    /* ISR already pushed bytes to ring buffer via Protocol_ParseByte.
     * Main loop just calls Protocol_ProcessBuffer() to drain the ring.
     * 此函数保留仅为 ABI 兼容, 不做任何事情. */
    (void)0;
}

void Protocol_USART1_RxCpltCallback(void)
{
    /* Same as above: ISR has already pushed bytes to ring buffer. */
    (void)0;
}

/* ================== Parse jinliao frame (10 bytes, module_id=0x01) ==================
 * 协议 §1.3: AA 55 01 05 omega1_LE(2B) 保留(2B) ir(1B) XOR(1B)
 * 协议仅 omega1 一路 (无 omega2), JinliaoData_TypeDef.omega2 已删除
 * ===================================================================================== */
static void ParseJinliaoFrame(uint8_t uart_idx, uint8_t *frame)
{
    uint16_t omega1_raw = ((uint16_t)frame[5] << 8) | frame[4];

    g_jinliao_data.omega1       = omega1_raw / 1000.0f;
    g_jinliao_data.fish_arrived = frame[8];
    g_jinliao_data.module_id    = frame[2];
    g_jinliao_data.valid        = 1;
    g_jinliao_data.timestamp    = HAL_GetTick();

    {
        uint32_t i;
        for (i = 0; i < 10; i++) g_rx_last_frame_jinliao[i] = frame[i];
    }
    g_rx_uart_idx_jinliao = uart_idx;
    g_rx_frame_valid_jinliao = 1;
    g_rx_count_jinliao++;
}

/* ================== Parse F103jinliao02 frame (12 bytes, module_id=0x02) ==================
 * 协议 §2.3: AA 55 02 07 omega_LE(2B) pressure_LE(2B) fish_drop(1B) 0(2B) XOR(1B)
 * ========================================================================================= */
static void ParseJinliao02Frame(uint8_t uart_idx, uint8_t *frame)
{
    uint8_t xor_sum = frame[2] ^ frame[3] ^ frame[4] ^ frame[5] ^
                      frame[6] ^ frame[7] ^ frame[8] ^ frame[9] ^ frame[10];
    if (xor_sum != frame[11]) {
        /* XOR 校验失败, 保留上一帧 */
        return;
    }

    {
        uint32_t i;
        for (i = 0; i < 12; i++) g_rx_last_frame_jinliao02[i] = frame[i];
    }
    g_rx_uart_idx_jinliao02 = uart_idx;
    g_rx_frame_valid_jinliao02 = 1;
    g_rx_count_jinliao02++;

    /* 填充业务数据结构 */
    uint16_t omega_raw    = ((uint16_t)frame[5] << 8) | frame[4];
    uint16_t pressure_raw = ((uint16_t)frame[7] << 8) | frame[6];

    g_jinliao02_data.omega        = omega_raw / 1000.0f;
    g_jinliao02_data.pressure_kpa = pressure_raw / 100.0f;
    g_jinliao02_data.fish_drop    = frame[8];
    g_jinliao02_data.module_id    = frame[2];
    g_jinliao02_data.valid        = 1;
    g_jinliao02_data.timestamp    = HAL_GetTick();
}

/* ================== Parse F407HALL frame (16 bytes, module_id=0x03) ==================
 * AA 55 03 0B omega1_LE(2B) omega2_LE(2B) omega3_LE(2B)
 *             dist1_LE(2B) dist2_LE(2B) rs(1B) xor(1B)
 * ========================================================================================= */
uint8_t Protocol_ParseHallVL6180XFrame(const uint8_t *frame)
{
    uint8_t xor_sum = 0;
    uint32_t k;

    if (frame[0] != FRAME_HEADER1 || frame[1] != FRAME_HEADER2 ||
        frame[2] != MODULE_ID_HALL_VL6180X ||
        frame[3] != PAYLOAD_HALL_VL6180X_LEN) {
        return 0U;
    }

    for (k = 2; k < (FRAME_HALL_VL6180X_LEN - 1U); k++) xor_sum ^= frame[k];

    if (xor_sum != frame[FRAME_HALL_VL6180X_LEN - 1U]) return 0U;

    {
        uint32_t i;
        for (i = 0; i < FRAME_HALL_VL6180X_LEN; i++) {
            g_rx_last_frame_hall_vl6180x[i] = frame[i];
        }
    }
    g_rx_uart_idx_hall_vl6180x = 0xFFU;
    g_rx_frame_valid_hall_vl6180x = 1;
    s_hall_vl6180x_timestamp = HAL_GetTick();
    g_rx_count_hall_vl6180x++;
    return 1U;
}

/* ================== Parse F407HALL01 I2C3 frame (17 bytes, module_id=0x04) ==================
 * AA 55 04 0C omega1_LE(2B) omega2_LE(2B) dist1_LE(2B) dist2_LE(2B)
 * dist3_LE(2B) range_status_LE(2B) xor(1B)
 * 此函数由 USART2 UART 状态机调用。
 * ========================================================================================= */
uint8_t Protocol_ParseHall01I2CFrame(const uint8_t *frame)
{
    uint8_t xor_sum = 0;
    uint32_t k;

    if (frame == NULL ||
        frame[0] != FRAME_HEADER1 || frame[1] != FRAME_HEADER2 ||
        frame[2] != MODULE_ID_HALL01_I2C || frame[3] != 0x0CU) {
        return 0U;
    }

    for (k = 2; k < 16; k++) xor_sum ^= frame[k];

    if (xor_sum != frame[16]) {
        /* XOR 校验失败, 保留上一帧 */
        return 0U;
    }

    /* 填充业务数据结构 */
    uint16_t omega1_raw = ((uint16_t)frame[5] << 8) | frame[4];
    uint16_t omega2_raw = ((uint16_t)frame[7] << 8) | frame[6];

    g_hall01_i2c_data.omega_mrad[0] = (int32_t)omega1_raw;
    g_hall01_i2c_data.omega_mrad[1] = (int32_t)omega2_raw;
    g_hall01_i2c_data.dist1_mm      = (uint16_t)((uint16_t)frame[8] | ((uint16_t)frame[9] << 8));
    g_hall01_i2c_data.dist2_mm      = (uint16_t)((uint16_t)frame[10] | ((uint16_t)frame[11] << 8));
    g_hall01_i2c_data.dist3_mm      = (uint16_t)((uint16_t)frame[12] | ((uint16_t)frame[13] << 8));
    g_hall01_i2c_data.range_status  = (uint16_t)((uint16_t)frame[14] | ((uint16_t)frame[15] << 8));
    g_hall01_i2c_data.module_id    = frame[2];
    g_hall01_i2c_data.valid        = 1;
    g_hall01_i2c_data.timestamp    = HAL_GetTick();
    return 1U;
}

/* ================== Parse chuliao frame (12 bytes) ================== */
static void ParseChuliaoFrame(uint8_t uart_idx, uint8_t *frame)
{
    uint16_t omega_raw    = ((uint16_t)frame[5] << 8) | frame[4];
    uint16_t pressure_raw = (uint16_t)frame[6] | ((uint16_t)frame[7] << 8);

    g_chuliao_data.omega         = omega_raw / 1000.0f;
    g_chuliao_data.pressure_kpa  = pressure_raw / 100.0f;
    g_chuliao_data.fish_drop     = frame[8];
    g_chuliao_data.module_id     = frame[2];
    g_chuliao_data.valid         = 1;
    g_chuliao_data.timestamp     = HAL_GetTick();

    {
        uint32_t i;
        for (i = 0; i < 12; i++) g_rx_last_frame_chuliao[i] = frame[i];
    }
    g_rx_uart_idx_chuliao = uart_idx;
    g_rx_frame_valid_chuliao = 1;
    g_rx_count_chuliao++;
}

/* ================== State machine: parse 1 byte ================== */
static void Protocol_ProcessByte(uint8_t uart_idx, uint8_t byte)
{
    g_debug_parse_state = s_parse_state[uart_idx];
    g_debug_frame_len   = s_frame_len[uart_idx];

    switch (s_parse_state[uart_idx]) {
        case 0:
            if (byte == FRAME_HEADER1) {
                s_frame_buf[uart_idx][0] = byte;
                s_frame_len[uart_idx] = 1;
                s_parse_state[uart_idx] = 1;
            }
            break;

        case 1:
            if (byte == FRAME_HEADER2) {
                s_frame_buf[uart_idx][1] = byte;
                s_frame_len[uart_idx] = 2;
                s_parse_state[uart_idx] = 2;
            } else if (byte == FRAME_HEADER1) {
                /* re-sync */
                s_frame_buf[uart_idx][0] = byte;
                s_frame_len[uart_idx] = 1;
            } else {
                s_parse_state[uart_idx] = 0;
                s_frame_len[uart_idx] = 0;
            }
            break;

        case 2:
            /* fb[2] 字节:
             *   F103 jinliao 帧   : fb[2]=0x01 (module_id), fb[3]=0x05 (payload len)
             *   F103 jinliao02 帧 : fb[2]=0x02 (module_id), fb[3]=0x07 (payload len)
             *   F103 chuliao 帧  : fb[2]=0x05 (module_id), fb[3]=0x07 (payload len)
             *   F407HALL 帧      : fb[2]=0x03 (module_id), fb[3]=0x0B (payload len)
             */
            s_frame_buf[uart_idx][2] = byte;
            s_frame_len[uart_idx] = 3;
            g_debug_module_id = byte;

            if (byte == MODULE_ID_CHULIAO) {
                /* F103 chuliao 帧, fb[2]=0x05, 直接定长 */
                s_expected_len[uart_idx] = FRAME_CHULIAO_LEN;
                s_parse_state[uart_idx] = 3;
            } else if (byte == MODULE_ID_JINLIAO02) {
                /* F103jinliao02 帧 (12 字节, module_id=0x02), XOR 校验 */
                s_expected_len[uart_idx] = FRAME_CHULIAO_LEN; /* 0x02 与 0x05 帧长相同: 12B */
                s_parse_state[uart_idx] = 3;
            } else if (byte == MODULE_ID_HALL_VL6180X) {
                /* F407HALL 帧 (16 字节, module_id=0x03), XOR 校验 */
                s_expected_len[uart_idx] = FRAME_HALL_VL6180X_LEN;
                s_parse_state[uart_idx] = 3;
            } else if (byte == MODULE_ID_HALL01_I2C) {
                s_expected_len[uart_idx] = FRAME_HALL01_I2C_LEN;
                s_parse_state[uart_idx] = 3;
            } else if (byte == 0x01U) {
                /* F103 jinliao: 等待 fb[3]=0x05 决定 */
                s_parse_state[uart_idx] = 5;
            } else {
                s_parse_state[uart_idx] = 0;
                s_frame_len[uart_idx] = 0;
            }
            break;

        case 5:
            /* fb[3] 字节:
             *   F103 jinliao (期望 10 字节) : fb[3]=0x05 (payload len)
             */
            s_frame_buf[uart_idx][3] = byte;
            s_frame_len[uart_idx] = 4;

            if (byte == 0x05U) {
                /* F103 jinliao */
                s_expected_len[uart_idx] = FRAME_JINLIAO_LEN;
                s_parse_state[uart_idx] = 3;
            } else {
                s_parse_state[uart_idx] = 0;
                s_frame_len[uart_idx] = 0;
            }
            break;

        case 3:
            if (s_frame_len[uart_idx] < s_expected_len[uart_idx]) {
                s_frame_buf[uart_idx][s_frame_len[uart_idx]] = byte;
                s_frame_len[uart_idx]++;
            }

            if (s_frame_len[uart_idx] >= s_expected_len[uart_idx]) {
                uint8_t exp_len = s_expected_len[uart_idx];
                uint8_t *fb = s_frame_buf[uart_idx];
                uint8_t xor_sum = 0;
                uint8_t xor_ok;
                uint32_t k;

                for (k = 2; k < (uint32_t)(exp_len - 1); k++) {
                    xor_sum ^= fb[k];
                }
                xor_ok = (xor_sum == fb[exp_len - 1]) ? 1 : 0;

                if (exp_len == FRAME_HALL_VL6180X_LEN && xor_ok) {
                    /* F407HALL 帧 (0x03, 16B) */
                    (void)Protocol_ParseHallVL6180XFrame(fb);
                } else if (exp_len == FRAME_HALL01_I2C_LEN && xor_ok) {
                    (void)Protocol_ParseHall01I2CFrame(fb);
                } else if (exp_len == FRAME_CHULIAO_LEN && xor_ok) {
                    /* 12B 帧: 可能是 chuliao(0x05) 或 jinliao02(0x02), 靠 fb[2] 区分 */
                    if (fb[3] == 7U) {
                        if (fb[2] == MODULE_ID_CHULIAO) {
                            ParseChuliaoFrame(uart_idx, fb);
                        } else if (fb[2] == MODULE_ID_JINLIAO02) {
                            ParseJinliao02Frame(uart_idx, fb);
                        }
                    }
                } else if (xor_ok && exp_len == FRAME_JINLIAO_LEN && fb[3] == 5U) {
                    ParseJinliaoFrame(uart_idx, fb);
                }
                /* 注: XOR 失败时不清 valid, 保留上一帧有效数据
                 *     (NMEA 100ms 重发, 失去的只是一帧, 不影响总观感) */

                s_parse_state[uart_idx] = 0;
                s_frame_len[uart_idx] = 0;
                s_expected_len[uart_idx] = 0;
            }
            break;

        default:
            s_parse_state[uart_idx] = 0;
            s_frame_len[uart_idx] = 0;
            break;
    }
}

/* ================== Drain ring buffer (call from main loop) ================== */
void Protocol_ProcessBuffer(uint8_t uart_idx)
{
    if (uart_idx >= UART_COUNT) return;

    /* 消费掉整个 ring buffer (不只 1 字节, 否则 ISR 比 main 快时积压) */
    while (RxRing_Available(uart_idx) > 0) {
        Protocol_ProcessByte(uart_idx, RxRing_Pop(uart_idx));
    }
}

/* ================== NMEA checksum ================== */
static uint8_t CalcXOR(const char *s)
{
    uint8_t x = 0;
    while (*s) x ^= (uint8_t)*s++;
    return x;
}

static void Protocol_AppendChecksum(char *out_buf)
{
    char *star = out_buf;
    uint8_t cs;

    while (*star && *star != '*') star++;
    if (*star == '*') {
        cs = CalcXOR(out_buf + 1);
        sprintf(star + 1, "%02X\r\n", cs);
    }
}

/* ================== Pack jinliao NMEA ================== */
uint16_t Protocol_BuildJinliaoNMEA(char *out_buf, uint16_t max_len)
{
    int n;

    if (!g_jinliao_data.valid) return 0;

    n = snprintf(out_buf, max_len,
        "$JINLIAO,%.3f,%d,%lu*",
        (double)g_jinliao_data.omega1,
        g_jinliao_data.fish_arrived,
        (unsigned long)g_jinliao_data.timestamp);

    if (n > 0) {
        Protocol_AppendChecksum(out_buf);
        return (uint16_t)strlen(out_buf);
    }
    return 0;
}

/* ================== Pack chuliao NMEA ================== */
uint16_t Protocol_BuildChuliaoNMEA(char *out_buf, uint16_t max_len)
{
    int n;

    if (!g_chuliao_data.valid) return 0;

    n = snprintf(out_buf, max_len,
        "$CHULIAO,%.3f,%.2f,%d,%lu*",
        (double)g_chuliao_data.omega,
        (double)g_chuliao_data.pressure_kpa,
        g_chuliao_data.fish_drop,
        (unsigned long)g_chuliao_data.timestamp);

    if (n > 0) {
        Protocol_AppendChecksum(out_buf);
        return (uint16_t)strlen(out_buf);
    }
    return 0;
}

/* ================== Pack combined NMEA (全部 5 路传感器数据, v2 简化格式) ==================
 *  简化 NMEA 格式:
 *    $FR,<seq>,J,<o1>,<arv>,<v>;C,<o>,<prs>,<drp>,<v>;P,<o>,<prs>,<drp>,<v>;
 *               D,<o1>,<o2>,<o3>,<d1>,<d2>,<rng>,<v>;I,<o1>,<o2>,<d1>,<d2>,<d3>,<rng>,<v>*CS\r\n
 *
 *  与 v1 区别:
 *    - 帧头 $FR (2 字节) 替代 $FRAME (6 字节)
 *    - 字段名缩短 (omega1->o1, pressure_kpa->prs, module_id->mid 等)
 *    - module_id 用十进制 1/2/3/4/5, 不写 0x...
 *    - 整包 ~150 字节 (v1 的 343 字节, 砍掉 56%)
 *
 *  校验算法修正 (v2):
 *    XOR 范围 = '$' 与 '*' 之间 (不含两端), 标准 NMEA.
 *    v1 算法会把末尾 '*' 算进 XOR, 导致 CS 永远差 0x2A.
 * ============================================================================= */
static uint8_t CalcXOR_Range(const char *start, const char *end_excl)
{
    uint8_t x = 0;
    while (start < end_excl) {
        x ^= (uint8_t)*start++;
    }
    return x;
}

uint16_t Protocol_BuildCombinedNMEA(char *out_buf, uint16_t max_len)
{
    int n;
    static uint16_t s_seq = 0;

    /* J 路 (jinliao01) */
    int j_o1 = g_jinliao_data.valid ? (int)(g_jinliao_data.omega1 * 1000) : 0;
    int j_arv = g_jinliao_data.valid ? (int)g_jinliao_data.fish_arrived : 0;
    int j_v = g_jinliao_data.valid ? 1 : 0;

    /* C 路 (chuliao05) */
    int c_o = g_chuliao_data.valid ? (int)(g_chuliao_data.omega * 1000) : 0;
    int c_prs = g_chuliao_data.valid ? (int)(g_chuliao_data.pressure_kpa * 100) : 0;
    int c_drp = g_chuliao_data.valid ? (int)g_chuliao_data.fish_drop : 0;
    int c_v = g_chuliao_data.valid ? 1 : 0;

    /* P 路 (jinliao02) */
    int p_o = g_jinliao02_data.valid ? (int)(g_jinliao02_data.omega * 1000) : 0;
    int p_prs = g_jinliao02_data.valid ? (int)(g_jinliao02_data.pressure_kpa * 100) : 0;
    int p_drp = g_jinliao02_data.valid ? (int)g_jinliao02_data.fish_drop : 0;
    int p_v = g_jinliao02_data.valid ? 1 : 0;

    /* D 路 (F407HALL/VL6180X) */
    long d_o1 = g_rx_frame_valid_hall_vl6180x ?
                (long)((uint16_t)((g_rx_last_frame_hall_vl6180x[5]<<8)|g_rx_last_frame_hall_vl6180x[4])) : 0L;
    long d_o2 = g_rx_frame_valid_hall_vl6180x ?
                (long)((uint16_t)((g_rx_last_frame_hall_vl6180x[7]<<8)|g_rx_last_frame_hall_vl6180x[6])) : 0L;
    long d_o3 = g_rx_frame_valid_hall_vl6180x ?
                (long)((uint16_t)((g_rx_last_frame_hall_vl6180x[9]<<8)|g_rx_last_frame_hall_vl6180x[8])) : 0L;
    unsigned d_d1 = g_rx_frame_valid_hall_vl6180x ?
                (unsigned)((g_rx_last_frame_hall_vl6180x[11]<<8)|g_rx_last_frame_hall_vl6180x[10]) : 0u;
    unsigned d_d2 = g_rx_frame_valid_hall_vl6180x ?
                (unsigned)((g_rx_last_frame_hall_vl6180x[13]<<8)|g_rx_last_frame_hall_vl6180x[12]) : 0u;
    unsigned d_rng = g_rx_frame_valid_hall_vl6180x ?
                (unsigned)g_rx_last_frame_hall_vl6180x[14] : 0u;
    int d_v = g_rx_frame_valid_hall_vl6180x ? 1 : 0;

    /* I 路 (F407HALL01 I2C) */
    long i_o1 = g_hall01_i2c_data.valid ? (long)g_hall01_i2c_data.omega_mrad[0] : 0L;
    long i_o2 = g_hall01_i2c_data.valid ? (long)g_hall01_i2c_data.omega_mrad[1] : 0L;
    unsigned i_d1 = g_hall01_i2c_data.valid ? (unsigned)g_hall01_i2c_data.dist1_mm : 0u;
    unsigned i_d2 = g_hall01_i2c_data.valid ? (unsigned)g_hall01_i2c_data.dist2_mm : 0u;
    unsigned i_d3 = g_hall01_i2c_data.valid ? (unsigned)g_hall01_i2c_data.dist3_mm : 0u;
    unsigned i_rng = g_hall01_i2c_data.valid ? (unsigned)g_hall01_i2c_data.range_status : 0u;
    int i_v = g_hall01_i2c_data.valid ? 1 : 0;

    n = snprintf(out_buf, max_len,
        "$FR,%u,J,%d,%d,%d;C,%d,%d,%d,%d;P,%d,%d,%d,%d;"
        "D,%ld,%ld,%ld,%u,%u,%u,%d;I,%ld,%ld,%u,%u,%u,%u,%d*",
        s_seq++,
        j_o1, j_arv, j_v,
        c_o, c_prs, c_drp, c_v,
        p_o, p_prs, p_drp, p_v,
        d_o1, d_o2, d_o3, d_d1, d_d2, d_rng, d_v,
        i_o1, i_o2, i_d1, i_d2, i_d3, i_rng, i_v);

    if (n <= 0) return 0;

    /* v2 校验算法: XOR 范围 = '$' 与 '*' 之间 (不含两端) */
    {
        char *dollar = NULL;
        char *star = NULL;
        char *p;
        uint8_t cs;
        for (p = out_buf; *p; p++) {
            if (!dollar && *p == '$') dollar = p;
            else if (*p == '*') { star = p; break; }
        }
        if (!dollar || !star || star <= dollar + 1) return 0;
        cs = CalcXOR_Range(dollar + 1, star);
        /* 把 *CS\r\n 写到 star 后面 */
        sprintf(star + 1, "%02X\r\n", cs);
    }

    return (uint16_t)strlen(out_buf);
}

static uint16_t scaled_u16(float value, float scale)
{
    float scaled = value * scale;
    if (scaled <= 0.0f) return 0U;
    if (scaled >= 65535.0f) return 65535U;
    return (uint16_t)scaled;
}

void Protocol_ExpireStaleData(uint32_t now_ms, uint32_t timeout_ms)
{
    if (g_jinliao_data.valid && (uint32_t)(now_ms - g_jinliao_data.timestamp) > timeout_ms) {
        g_jinliao_data.omega1 = 0.0f;
        g_jinliao_data.fish_arrived = 0U;
        g_jinliao_data.module_id = 0U;
        g_jinliao_data.valid = 0U;
        g_rx_frame_valid_jinliao = 0U;
    }
    if (g_chuliao_data.valid && (uint32_t)(now_ms - g_chuliao_data.timestamp) > timeout_ms) {
        g_chuliao_data.omega = 0.0f;
        g_chuliao_data.pressure_kpa = 0.0f;
        g_chuliao_data.fish_drop = 0U;
        g_chuliao_data.module_id = 0U;
        g_chuliao_data.valid = 0U;
        g_rx_frame_valid_chuliao = 0U;
    }
    if (g_jinliao02_data.valid && (uint32_t)(now_ms - g_jinliao02_data.timestamp) > timeout_ms) {
        g_jinliao02_data.omega = 0.0f;
        g_jinliao02_data.pressure_kpa = 0.0f;
        g_jinliao02_data.fish_drop = 0U;
        g_jinliao02_data.module_id = 0U;
        g_jinliao02_data.valid = 0U;
        g_rx_frame_valid_jinliao02 = 0U;
    }
    if (g_rx_frame_valid_hall_vl6180x &&
        (uint32_t)(now_ms - s_hall_vl6180x_timestamp) > timeout_ms) {
        uint8_t i;
        for (i = 0U; i < FRAME_HALL_VL6180X_LEN; ++i) g_rx_last_frame_hall_vl6180x[i] = 0U;
        g_rx_frame_valid_hall_vl6180x = 0U;
    }
    if (g_hall01_i2c_data.valid &&
        (uint32_t)(now_ms - g_hall01_i2c_data.timestamp) > timeout_ms) {
        g_hall01_i2c_data.omega_mrad[0] = 0;
        g_hall01_i2c_data.omega_mrad[1] = 0;
        g_hall01_i2c_data.dist1_mm = 0U;
        g_hall01_i2c_data.dist2_mm = 0U;
        g_hall01_i2c_data.dist3_mm = 0U;
        g_hall01_i2c_data.range_status = 0U;
        g_hall01_i2c_data.module_id = 0U;
        g_hall01_i2c_data.valid = 0U;
    }
}

void Protocol_BuildModbusInputRegisters(uint16_t *registers, uint16_t count,
                                        uint16_t sequence, uint8_t command_fresh)
{
    uint16_t valid = 0U;
    uint16_t i;
    if (registers == 0 || count < 30U) return;
    for (i = 0U; i < count; ++i) registers[i] = 0U;
    registers[0] = 0x0100U;
    registers[1] = sequence;
    if (g_jinliao_data.valid) valid |= 0x01U;
    if (g_chuliao_data.valid) valid |= 0x02U;
    if (g_jinliao02_data.valid) valid |= 0x04U;
    if (g_rx_frame_valid_hall_vl6180x) valid |= 0x08U;
    if (g_hall01_i2c_data.valid) valid |= 0x10U;
    registers[2] = valid;
    registers[3] = (uint16_t)(0x0001U | (command_fresh ? 0x0002U : 0U));
    if (valid & 0x01U) {
        registers[10] = scaled_u16(g_jinliao_data.omega1, 1000.0f);
        registers[11] = g_jinliao_data.fish_arrived ? 1U : 0U;
    }
    if (valid & 0x02U) {
        registers[12] = scaled_u16(g_chuliao_data.omega, 1000.0f);
        registers[13] = scaled_u16(g_chuliao_data.pressure_kpa, 100.0f);
        registers[14] = g_chuliao_data.fish_drop ? 1U : 0U;
    }
    if (valid & 0x04U) {
        registers[15] = scaled_u16(g_jinliao02_data.omega, 1000.0f);
        registers[16] = scaled_u16(g_jinliao02_data.pressure_kpa, 100.0f);
        registers[17] = g_jinliao02_data.fish_drop ? 1U : 0U;
    }
    if (valid & 0x08U) {
        registers[18] = (uint16_t)((g_rx_last_frame_hall_vl6180x[5] << 8) | g_rx_last_frame_hall_vl6180x[4]);
        registers[19] = (uint16_t)((g_rx_last_frame_hall_vl6180x[7] << 8) | g_rx_last_frame_hall_vl6180x[6]);
        registers[20] = (uint16_t)((g_rx_last_frame_hall_vl6180x[9] << 8) | g_rx_last_frame_hall_vl6180x[8]);
        registers[21] = (uint16_t)((g_rx_last_frame_hall_vl6180x[11] << 8) | g_rx_last_frame_hall_vl6180x[10]);
        registers[22] = (uint16_t)((g_rx_last_frame_hall_vl6180x[13] << 8) | g_rx_last_frame_hall_vl6180x[12]);
        registers[23] = g_rx_last_frame_hall_vl6180x[14];
    }
    if (valid & 0x10U) {
        registers[24] = (uint16_t)g_hall01_i2c_data.omega_mrad[0];
        registers[25] = (uint16_t)g_hall01_i2c_data.omega_mrad[1];
        registers[26] = g_hall01_i2c_data.dist1_mm;
        registers[27] = g_hall01_i2c_data.dist2_mm;
        registers[28] = g_hall01_i2c_data.dist3_mm;
        registers[29] = g_hall01_i2c_data.range_status;
    }
}
