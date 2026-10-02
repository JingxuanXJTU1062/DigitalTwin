/**
  ******************************************************************************
  * @file    usart_protocol.h
  * @brief   UART protocol - Receive F103 data and pack into NMEA strings
  *
  *  Frame format (must match F103 sender code):
  *    Header  : 0xAA 0x55
  *    Module  : 0x01 (jinliao) | 0x05 (chuliao)
  *    Length  : 1 byte, payload length (skip)
 *    Payload : little-endian uint16 fields (low byte first); Hall speed is unsigned mrad/s
  *    XOR     : XOR of bytes from MODULE_ID to last payload byte
  *
  *  jinliao frame (10 bytes, total length 10):
  *    [0]  0xAA        [1] 0x55       [2] 0x01
  *    [3]  PAYLOAD_LEN=5
  *    [4]  omega1_L    [5] omega1_H
  *    [6]  reserved_L  [7] reserved_H
  *    [8]  fish_arrived
  *    [9]  XOR
  *
  *  chuliao frame (12 bytes, total length 12):
  *    [0]  0xAA        [1] 0x55       [2] 0x05
  *    [3]  PAYLOAD_LEN=7
  *    [4]  omega_L     [5] omega_H
  *    [6]  pressure_L  [7] pressure_H
  *    [8]  fish_drop
  *    [9]  0x00 (reserved)
  *    [10] 0x00 (reserved)
  *    [11] XOR
  ******************************************************************************
  */
#ifndef __USART_PROTOCOL_H
#define __USART_PROTOCOL_H

#include "stm32f4xx_hal.h"

/* ================== 帧格式常量 ================== */
#define FRAME_HEADER1      0xAA
#define FRAME_HEADER2      0x55

#define MODULE_ID_JINLIAO  0x01
#define MODULE_ID_CHULIAO  0x05
#define MODULE_ID_JINLIAO02 0x02   /* F103 进料2 (jinliao02, 霍尔+压力) */
#define MODULE_ID_HALL_VL6180X 0x03 /* F407HALL (3 Hall + 2 VL6180X, 16B) */
#define MODULE_ID_HALL01_I2C  0x04  /* F407HALL03 USART1 (17B) */

#define FRAME_JINLIAO_LEN     10
#define FRAME_CHULIAO_LEN     12
#define FRAME_HALL_VL6180X_LEN 16    /* F407HALL (0x03): AA55(2) + id(1) + len(1) + data(11) + xor(1) */
#define PAYLOAD_HALL_VL6180X_LEN 0x0B
#define FRAME_HALL01_I2C_LEN  17     /* F407HALL03 (0x04) via USART2 */

/* ================== UART 通道 ================== */
/* 5 路 UART 真值表 (索引号 0..3, 与硬件 UART 一一对应)
 *   UART_IDX_UART4      = 0   UART4   (PC10/PC11)   <- F103jinliao01 (0x01 进料)
 *   UART_IDX_USART1     = 1   USART1  (PA9/PA10)    <- F103jinliao02  (0x02 进料2: 霍尔+压力)
 *   UART_IDX_USART2     = 2   USART2  (PA2/PA3)     <- F407HALL03     (0x04 2路霍尔+3路测距)
 *   UART_IDX_UART5_HALL = 3   UART5   (PD2 only)    <- F103chuliao05  (0x05 出料)
 */
#define UART_IDX_UART4      0
#define UART_IDX_USART1     1
#define UART_IDX_USART2     2
#define UART_IDX_UART5_HALL 3
#define UART_COUNT          4       /* UART4 + USART1 + USART2 + UART5 */

/* ================== Ring buffer ================== */
/* 1024 字节足以缓冲主循环在 ESP8266 阻塞 AT 命令期间 (最长 ~100ms)
 * 由 USART1 + UART4 两个通道持续 push 的数据
 * F103 @ 115200 每帧 10/12 字节, 100ms ~1200 字节 -> 1024 接近临界
 * 但 ESP8266_Get_IdLinkStatus 的 waittime 已从 500ms 减至 100ms,
 * 且 main 循环每周期先 drain ring 再发 NMEA, 实际不会持续累积 */
#define UART_RX_RING_SIZE  1024

typedef struct {
    uint8_t data[UART_RX_RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RxRing_TypeDef;

/* ================== 数据结构体 ================== */
typedef struct {
    float    omega1;
    uint8_t  fish_arrived;
    uint8_t  module_id;
    uint8_t  valid;
    uint32_t timestamp;
} JinliaoData_TypeDef;

typedef struct {
    float    omega;
    float    pressure_kpa;
    uint8_t  fish_drop;
    uint8_t  module_id;
    uint8_t  valid;
    uint32_t timestamp;
} ChuliaoData_TypeDef;

/* F103jinliao02 (0x02) 数据结构体: 霍尔 + 压力 FSR */
typedef struct {
    float    omega;            /* mrad/s (rad/s × 1000) */
    float    pressure_kpa;     /* kPa */
    uint8_t  fish_drop;        /* 1=有鱼落下 */
    uint8_t  module_id;
    uint8_t  valid;
    uint32_t timestamp;
} Jinliao02Data_TypeDef;

/* F407HALL01 I2C3 (0x04) 数据结构体: 2路霍尔 + 3路 VL6180X */
typedef struct {
    int32_t  omega_mrad[2];   /* 霍尔通道 1,2 (mrad/s) */
    uint16_t dist1_mm;         /* VL6180X #1 距离 (mm) */
    uint16_t dist2_mm;         /* VL6180X #2 距离 (mm) */
    uint16_t dist3_mm;         /* VL6180X #3 距离 (mm) */
    uint16_t range_status;     /* bit[3:0]/[7:4]/[11:8]=传感器1/2/3 */
    uint8_t  module_id;
    uint8_t  valid;
    uint32_t timestamp;
} Hall01I2CData_TypeDef;

/* ================== 全局变量 (供 Watch 窗口观察) ================== */
extern volatile JinliaoData_TypeDef g_jinliao_data;
extern volatile ChuliaoData_TypeDef g_chuliao_data;
extern volatile Jinliao02Data_TypeDef g_jinliao02_data;
extern volatile Hall01I2CData_TypeDef g_hall01_i2c_data;
extern volatile uint8_t  g_rx_last_frame_jinliao[10];
extern volatile uint8_t  g_rx_last_frame_chuliao[12];
extern volatile uint8_t  g_rx_last_frame_jinliao02[12];   /* F103jinliao02 (0x02) */
extern volatile uint8_t  g_rx_last_frame_hall_vl6180x[FRAME_HALL_VL6180X_LEN]; /* F407HALL (0x03) */
extern volatile uint8_t  g_rx_frame_valid_jinliao;
extern volatile uint8_t  g_rx_frame_valid_chuliao;
extern volatile uint8_t  g_rx_frame_valid_jinliao02;     /* F103jinliao02 */
extern volatile uint8_t  g_rx_frame_valid_hall_vl6180x; /* F407HALL */
extern volatile uint8_t  g_rx_uart_idx_jinliao;
extern volatile uint8_t  g_rx_uart_idx_chuliao;
extern volatile uint8_t  g_rx_uart_idx_jinliao02;      /* F103jinliao02 */
extern volatile uint8_t  g_rx_uart_idx_hall_vl6180x;   /* F407HALL */
extern volatile uint32_t g_rx_count_jinliao;
extern volatile uint32_t g_rx_count_chuliao;
extern volatile uint32_t g_rx_count_jinliao02;      /* F103jinliao02 */
extern volatile uint32_t g_rx_count_hall_vl6180x;  /* F407HALL */
extern volatile uint8_t  g_debug_last_byte;
extern volatile uint8_t  g_debug_parse_state;
extern volatile uint8_t  g_debug_module_id;
extern volatile uint8_t  g_debug_frame_len;

/* ================== API 函数 ================== */

/* ISR-side: store 1 byte (由 main 调用, 等价 F407_03 callback) */
void Protocol_ParseByte(uint8_t uart_idx, uint8_t byte);

/* main 循环调用: 从 s_rx_byte 推入 ring buffer */
void Protocol_UART4_RxCpltCallback(void);
void Protocol_USART1_RxCpltCallback(void);

/* main 循环调用: 状态机解析 1 字节 */
void Protocol_ProcessBuffer(uint8_t uart_idx);

/* main 循环调用: 打包 NMEA 字符串 (为空返回 0) */
uint16_t Protocol_BuildJinliaoNMEA(char *out_buf, uint16_t max_len);
uint16_t Protocol_BuildChuliaoNMEA(char *out_buf, uint16_t max_len);

/* 合并 jinliao + chuliao + 两路测距到一条 NMEA, 一次性 TCP 发送 */
uint16_t Protocol_BuildCombinedNMEA(char *out_buf, uint16_t max_len);

/* 解析 I2C3 0x04 帧 (F407HALL01) */
/* 返回 1=帧头、长度和 XOR 均正确，0=无效帧。 */
uint8_t Protocol_ParseHall01I2CFrame(const uint8_t *frame);
uint8_t Protocol_ParseHallVL6180XFrame(const uint8_t *frame);

/* Modbus TCP 快照与陈旧数据处理。 */
void Protocol_ExpireStaleData(uint32_t now_ms, uint32_t timeout_ms);
void Protocol_BuildModbusInputRegisters(uint16_t *registers, uint16_t count,
                                        uint16_t sequence, uint8_t command_fresh);

#endif /* __USART_PROTOCOL_H */
