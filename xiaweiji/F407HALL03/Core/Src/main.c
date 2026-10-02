/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hall.h"
#include "vl6180x.h"
#include "uart_tx.h"
#include "i2c.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* VL6180X distance sensor */
static VL6180X_HandleTypeDef g_vl6180x_1;
static VL6180X_HandleTypeDef g_vl6180x_2;
static VL6180X_HandleTypeDef g_vl6180x_3;
volatile uint8_t g_distance_mm = 0U;
volatile uint8_t g_range_status = 0U;
volatile uint8_t g_distance_mm_2 = 0U;
volatile uint8_t g_range_status_2 = 0U;
volatile uint8_t g_sensor_present = 0U;
volatile uint8_t g_model_id = 0U;
volatile uint32_t g_stage1_error = 0U;
volatile uint8_t g_sensor2_present = 0U;
volatile uint8_t g_model_id_2 = 0U;
volatile uint32_t g_stage1_error_2 = 0U;
volatile uint8_t g_distance_mm_3 = 0U;
volatile uint8_t g_range_status_3 = 0U;
volatile uint8_t g_sensor3_present = 0U;
volatile uint8_t g_model_id_3 = 0U;
volatile uint32_t g_stage1_error_3 = 0U;
volatile uint8_t g_measure_status_3 = VL6180X_STATUS_INVALID_ARGUMENT;
volatile uint8_t g_measure_stage_3 = VL6180X_MEASURE_STAGE_IDLE;
volatile uint32_t g_measure_ok_count_3 = 0U;
volatile uint32_t g_measure_error_count_3 = 0U;
volatile uint32_t g_i2c3_recovery_count = 0U;
volatile uint32_t g_i2c3_error_at_failure = HAL_I2C_ERROR_NONE;
static uint32_t g_i2c3_last_recovery_tick = 0U;

/* 启动阶段标记: 1=main入口 2=HAL初始化 3=时钟 4=外设 5=应用层就绪 */
volatile uint32_t g_boot_stage = 0U;
volatile uint32_t g_error_handler_count = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Stage1_ResetSensors(void);
static void Stage1_CheckSensor(VL6180X_HandleTypeDef *sensor, I2C_HandleTypeDef *i2c,
                               volatile uint8_t *present, volatile uint8_t *model_id,
                               volatile uint32_t *app_error);
static VL6180X_StatusTypeDef Stage1_MeasureOnce(VL6180X_HandleTypeDef *sensor,
                                                volatile uint8_t *distance_mm,
                                                volatile uint8_t *range_status);
static void Stage1_RecoverSensor3(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Stage1_ResetSensors(void)
{
  HAL_GPIO_WritePin(VL6180X_SHDN_1_GPIO_Port, VL6180X_SHDN_1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(VL6180X_SHDN_2_GPIO_Port, VL6180X_SHDN_2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(VL6180X_SHDN_3_GPIO_Port, VL6180X_SHDN_3_Pin, GPIO_PIN_RESET);
  HAL_Delay(10U);
}

static void Stage1_CheckSensor(VL6180X_HandleTypeDef *sensor, I2C_HandleTypeDef *i2c,
                               volatile uint8_t *present, volatile uint8_t *model_id,
                               volatile uint32_t *app_error)
{
  *present = 0U;
  *model_id = 0U;
  *app_error = 0U;
  
  VL6180X_StatusTypeDef status = VL6180X_Init(sensor, i2c);
  *model_id = sensor->model_id;
  
  if (status == VL6180X_STATUS_I2C_ERROR) {
    *app_error = 1U;  /* 传感器未找到 */
    return;
  }
  
  if (status == VL6180X_STATUS_WRONG_DEVICE) {
    *app_error = 2U;  /* 错误的设备 */
    return;
  }
  if (status != VL6180X_STATUS_OK) {
    *app_error = 3U;  /* 初始化失败 */
    return;
  }
  *present = 1U;
}

static VL6180X_StatusTypeDef Stage1_MeasureOnce(VL6180X_HandleTypeDef *sensor,
                                                volatile uint8_t *distance_mm,
                                                volatile uint8_t *range_status)
{
  uint8_t distance = 0U;
  *range_status = 0U;
  
  if (sensor->initialized == 0U) {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }
  
  VL6180X_StatusTypeDef status = VL6180X_ReadDistance(sensor, &distance);
  *range_status = sensor->range_status;
  
  if (status != VL6180X_STATUS_OK) {
    return status;
  }
  
  *distance_mm = distance;
  return VL6180X_STATUS_OK;
}

static void Stage1_RecoverSensor3(void)
{
  g_i2c3_recovery_count++;
  g_sensor3_present = 0U;
  g_vl6180x_3.initialized = 0U;

  HAL_GPIO_WritePin(VL6180X_SHDN_3_GPIO_Port, VL6180X_SHDN_3_Pin, GPIO_PIN_RESET);
  HAL_Delay(10U);
  (void)HAL_I2C_DeInit(&hi2c3);
  MX_I2C3_Init();
  HAL_GPIO_WritePin(VL6180X_SHDN_3_GPIO_Port, VL6180X_SHDN_3_Pin, GPIO_PIN_SET);
  HAL_Delay(50U);
  Stage1_CheckSensor(&g_vl6180x_3, &hi2c3, &g_sensor3_present,
                     &g_model_id_3, &g_stage1_error_3);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  g_boot_stage = 1U;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_I2C3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  g_boot_stage = 4U;

  UART_Tx_Init(&huart1);
  Stage1_ResetSensors();
  HAL_GPIO_WritePin(VL6180X_SHDN_1_GPIO_Port, VL6180X_SHDN_1_Pin, GPIO_PIN_SET);
  HAL_Delay(20U);
  Stage1_CheckSensor(&g_vl6180x_1, &hi2c1, &g_sensor_present, &g_model_id, &g_stage1_error);
  HAL_GPIO_WritePin(VL6180X_SHDN_2_GPIO_Port, VL6180X_SHDN_2_Pin, GPIO_PIN_SET);
  HAL_Delay(20U);
  Stage1_CheckSensor(&g_vl6180x_2, &hi2c2, &g_sensor2_present, &g_model_id_2, &g_stage1_error_2);
  HAL_GPIO_WritePin(VL6180X_SHDN_3_GPIO_Port, VL6180X_SHDN_3_Pin, GPIO_PIN_SET);
  HAL_Delay(50U);
  Stage1_CheckSensor(&g_vl6180x_3, &hi2c3, &g_sensor3_present, &g_model_id_3, &g_stage1_error_3);

  /* 霍尔传感器初始化 */
  Hall_Init();

  g_boot_stage = 5U;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 更新霍尔数据 (计算角速度) */
    Hall_Update();

    /* 获取霍尔角速度 */
    uint32_t omega1 = Hall_GetOmega(0);
    uint32_t omega2 = Hall_GetOmega(1);
    (void)Stage1_MeasureOnce(&g_vl6180x_1, &g_distance_mm, &g_range_status);
    (void)Stage1_MeasureOnce(&g_vl6180x_2, &g_distance_mm_2, &g_range_status_2);
    VL6180X_StatusTypeDef measure_status_3 =
        Stage1_MeasureOnce(&g_vl6180x_3, &g_distance_mm_3, &g_range_status_3);
    g_measure_status_3 = (uint8_t)measure_status_3;
    g_measure_stage_3 = g_vl6180x_3.measure_stage;
    if (measure_status_3 == VL6180X_STATUS_OK) {
      g_measure_ok_count_3++;
    } else {
      uint32_t now = HAL_GetTick();
      g_measure_error_count_3++;
      g_i2c3_error_at_failure = hi2c3.ErrorCode;
      if ((measure_status_3 == VL6180X_STATUS_I2C_ERROR) &&
          ((hi2c3.ErrorCode & HAL_I2C_ERROR_TIMEOUT) != 0U) &&
          ((g_i2c3_last_recovery_tick == 0U) ||
           ((now - g_i2c3_last_recovery_tick) >= 1000U))) {
        g_i2c3_last_recovery_tick = now;
        Stage1_RecoverSensor3();
      }
    }
    {
      uint16_t rs = (uint16_t)(g_range_status & 0x0FU) |
                    (uint16_t)((uint16_t)(g_range_status_2 & 0x0FU) << 4U) |
                    (uint16_t)((uint16_t)(g_range_status_3 & 0x0FU) << 8U);
      (void)UART_Tx_Send(omega1, omega2, (uint16_t)g_distance_mm,
                         (uint16_t)g_distance_mm_2, (uint16_t)g_distance_mm_3, rs);
    }

    HAL_Delay(50);
  /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  g_error_handler_count++;
  g_boot_stage = 0xE0000000U | g_boot_stage;
  __disable_irq();
  while (1)
  {
    /* 死循环 */
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
