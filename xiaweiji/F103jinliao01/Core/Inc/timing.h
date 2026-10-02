/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : timing.h
 * @brief          : 发送管理头文件 (不使用定时器，使用SysTick)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __TIMING_H
#define __TIMING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/*========================== 发送帧格式定义 ==========================*/
/* 帧结构: 0xAA 0x55 | 模块ID(0x01) | 长度(5) | 角速度1(u16小端) | 保留(u16) | 红外状态(u8) | XOR校验 */

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
extern volatile uint8_t g_tx_ready;   /* 发送就绪标志 */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
void Timing_Init(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __TIMING_H */
