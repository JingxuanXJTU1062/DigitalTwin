/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : timing.c
  * @brief          : 定时器驱动 (使用SysTick实现50ms周期)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "timing.h"
#include "sensor.h"
#include "usart_protocol.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/* Private variables ----------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile uint32_t g_sys_tick_1ms = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ----------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  定时器初始化
  */
void Timing_Init(void)
{
    g_sys_tick_1ms = 0;
}

/**
  * @brief  SysTick周期回调 (1ms一次)
  */
void HAL_SYSTICK_Callback(void)
{
    /* SysTick回调暂不用于传感器更新，由主循环轮询处理 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
