/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usart_protocol.h
  * @brief          : 串口协议头文件
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __USART_PROTOCOL_H
#define __USART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sensor.h"

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/*========================== 串口帧格式 ==========================*/
/*
 * 霍尔+压力模块帧格式 (12字节):
 * [0]  0xAA       帧头1
 * [1]  0x55       帧头2
 * [2]  模块ID     0x02(霍尔+压力模块)
 * [3]  长度       7(载荷长度)
 * [4]  omega_L    角速度低字节 (rad/s × 1000，即mrad/s)
 * [5]  omega_H    角速度高字节
 * [6]  pressure_L 压力低字节 (kPa × 100)
 * [7]  pressure_H 压力高字节
 * [8]  fish_drop  鱼体检测 (1=有鱼落下, 0=无鱼)
 * [9]  0x00       保留
 * [10] 0x00       保留
 * [11] XOR校验
 */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported variables --------------------------------------------------------*/
extern volatile uint8_t g_tx_ready;
extern volatile uint32_t g_tx_frame_count;  /* 发送帧计数 */

/* 可watch的发送缓冲 */
extern volatile uint8_t g_tx_frame[12];       /* 当前发送帧 */
extern volatile uint8_t g_tx_last_frame[12];   /* 最后发送的帧副本 */
extern volatile uint8_t g_tx_send_flag;        /* 发送标志 */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
void Protocol_Init(void);
void Protocol_SendFrame(void);
void Protocol_TxCpltCallback(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __USART_PROTOCOL_H */
