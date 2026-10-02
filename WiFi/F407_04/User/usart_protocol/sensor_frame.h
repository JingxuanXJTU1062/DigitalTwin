/**
  ******************************************************************************
  * @file    sensor_frame.h
 * @brief   5路传感器快照模块；F407HALL 使用 I2C3
  *
  *  5 路快照来源:
  *    [0] UART4 -> F103jinliao01  (0x01, 10B) → g_jinliao_data
  *    [1] USART1 -> F103jinliao02 (0x02, 12B) → g_jinliao02_data
  *    [2] UART5  -> F103chuliao05 (0x05, 12B) → g_chuliao_data
 *    [3] I2C3   -> F407HALL      (0x03, 16B) → g_rx_last_frame_hall_vl6180x
 *    [4] USART2 -> F407HALL03    (0x04, 17B) → g_hall01_i2c_data
  *
 *  本模块负责 I2C3 轮询 (SensorFrame_PollI2C3).
  *  其余 4 路 UART 数据由 usart_protocol.c 处理.
  ******************************************************************************
  */
#ifndef __SENSOR_FRAME_H
#define __SENSOR_FRAME_H

#include "stm32f4xx_hal.h"

/* ================== 外部全局变量 (来自 usart_protocol.c, 用于本模块读取 UART 帧) ================== */
extern volatile uint8_t  g_rx_last_frame_jinliao[10];
extern volatile uint8_t  g_rx_last_frame_chuliao[12];
extern volatile uint8_t  g_rx_last_frame_jinliao02[12];
extern volatile uint8_t  g_rx_last_frame_hall_vl6180x[16];
extern volatile uint8_t  g_rx_frame_valid_jinliao;
extern volatile uint8_t  g_rx_frame_valid_chuliao;
extern volatile uint8_t  g_rx_frame_valid_jinliao02;
extern volatile uint8_t  g_rx_frame_valid_hall_vl6180x;

/* ================== API ================== */

/* 初始化 (占位) */
void SensorFrame_Init(void);

/* 轮询 I2C3 并解析 0x03 帧（每 100ms 调一次）。 */
void SensorFrame_PollI2C3(void);

#endif /* __SENSOR_FRAME_H */
