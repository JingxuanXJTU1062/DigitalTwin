/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : timing.c
 * @brief          : 发送管理 (不使用定时器)
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

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* 发送就绪标志: 1=可以发送, 0=正在发送 */
volatile uint8_t g_tx_ready = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*========================== 发送管理函数实现 ==========================*/

/**
 * @brief  发送模块初始化
 */
void Timing_Init(void)
{
    g_tx_ready = 1;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
