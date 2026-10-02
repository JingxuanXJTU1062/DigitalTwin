/**
  ******************************************************************************
  * @file    wifi_cmd.h
  * @brief   主机 (PC) -> F407 命令接收模块 (经 ESP8266 AP 透传)
  *
  *  协议: NMEA 风格 $CMD,<f1>,<f2>,<f3>,<f4>*CS\r\n
  *        - f1/f2/f3/f4: 整数 0 或 1, 对应 GPIO 电平
  *        - CS: XOR 校验 (与 usart_protocol.c 中 CalcXOR 一致)
  *
  *  数据流:
  *    Legacy only: old $CMD parser retained for offline compatibility.
  *        --[USART3 +IPD,<id>,<len>:$CMD,...*CS\r\n]--> F407
  *
  *  集成:
  *    1. USART3_IRQHandler 调用 wifi_cmd_push_byte(byte)
  *    2. main 循环调用 wifi_cmd_process() (非阻塞)
  *    3. 启动时调用 wifi_cmd_init() 初始化 4 个 GPIO
  *
  *  GPIO 输出 (PE4/PE5/PE6/PC2):
  *    - 已避开 USART (PA9/10, PB10/11, PC10/11) 与全部 I2C 引脚
  *    - 已避开已被占用的引脚 (PE2 CH_PD, PE3 DHT11, PF6/7/8 RGB LED, PG15 RST)
  *
  *  Watch 观测 (Keil Watch 窗口):
  *    g_cmd_flag1/2/3/4 - 当前 GPIO 电平 (0/1)
  *    g_cmd_rx_count    - 成功应用命令数
  *    g_cmd_rx_bad_cs   - 校验和错误数
  *    g_cmd_rx_overflow - Ring buffer 溢出次数
  *    g_cmd_rx_total    - ISR push 进 ring 的总字节数 (含 ESP8266 头部垃圾)
  ******************************************************************************
  */
#ifndef __WIFI_CMD_H
#define __WIFI_CMD_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ================== Watch 变量 (供 Keil 实时观测) ================== */
extern volatile uint8_t  g_cmd_flag1;        /* PE4 当前电平 (0/1) */
extern volatile uint8_t  g_cmd_flag2;        /* PE5 当前电平 (0/1) */
extern volatile uint8_t  g_cmd_flag3;        /* PE6 当前电平 (0/1) */
extern volatile uint8_t  g_cmd_flag4;        /* PC2 当前电平 (0/1) */

/* 调试统计 */
extern volatile uint32_t g_cmd_rx_count;     /* 成功接收并应用的命令数 */
extern volatile uint32_t g_cmd_rx_bad_cs;    /* 校验和失败次数 */
extern volatile uint32_t g_cmd_rx_overflow;  /* Ring buffer 溢出次数 */
extern volatile uint32_t g_cmd_rx_total;     /* ISR push 进 ring 的总字节数 */

/* ================== API ================== */

/* 初始化 4 个 GPIO (PE4/PE5/PE6/PC2) 为推挽输出, 初始低电平 */
void wifi_cmd_init(void);

/* ISR 中调用: 把 1 字节推入 ring buffer (O(1), 不阻塞) */
void wifi_cmd_push_byte(uint8_t byte);

/* main 循环调用: 从 ring 取出字节, 解析 $CMD 帧, 设置 GPIO (非阻塞) */
void wifi_cmd_process(void);

/* Modbus线圈回调使用：index 0..3 对应 PE4/PE5/PE6/PC2。 */
void wifi_cmd_set_output(uint8_t index, uint8_t level);

#endif /* __WIFI_CMD_H */
