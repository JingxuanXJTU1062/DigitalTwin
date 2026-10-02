/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usart_protocol.h
 * @brief          : 串口通信协议头文件
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

/*========================== 串口帧协议定义 ==========================*/
/* 帧格式: 0xAA 0x55 | 模块ID | 长度 | 角速度1(u16 LE) | 保留(u16) | 红外状态(u8) | XOR */
/* 总长度: 2 + 1 + 1 + 2 + 2 + 1 + 1 = 10字节 */
#define PROTOCOL_HEAD_1       0xAA    /* 帧头1 */
#define PROTOCOL_HEAD_2       0x55    /* 帧头2 */
#define PROTOCOL_MODULE_ID    0x01    /* 进料模块ID */
#define PROTOCOL_PAYLOAD_LEN  5       /* 载荷长度: omega1(2) + reserved(2) + ir_status(1) */
#define PROTOCOL_TOTAL_LEN    10      /* 总帧长度 */

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
extern volatile uint8_t g_tx_ready;   /* 发送就绪标志 (在timing.c中定义) */
extern volatile uint32_t g_tx_frame_count;  /* 发送帧计数 */

/* 可watch的发送缓冲 */
extern volatile uint8_t g_tx_frame[10];       /* 当前发送帧 */
extern volatile uint8_t g_tx_last_frame[10];   /* 最后发送的帧副本 */

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
