/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : timing.h
  * @brief          : 定时器头文件
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

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

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
extern volatile uint32_t g_sys_tick_1ms;

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
void Timing_Init(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __TIMING_H */
