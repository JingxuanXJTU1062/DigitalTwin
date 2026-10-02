/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usart_protocol.c
 * @brief          : 串口通信协议实现
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usart_protocol.h"
#include "sensor.h"
#include "string.h"

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
extern UART_HandleTypeDef huart1;

/* 发送帧缓冲区 */
/* 帧格式: [0]0xAA [1]0x55 [2]ID [3]LEN [4-5]omega1 [6-7]reserved [8]ir_status [9]XOR */
volatile uint8_t g_tx_frame[10];
volatile uint8_t g_tx_last_frame[10];  /* 最后一次发送的帧副本，方便调试 */

/* 发送完成标志 */
static volatile uint8_t s_tx_complete = 1;

/* 发送计数 */
volatile uint32_t g_tx_frame_count;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*========================== 串口协议函数实现 ==========================*/

/**
 * @brief  协议模块初始化
 */
void Protocol_Init(void)
{
    s_tx_complete = 1;
    memset((uint8_t*)g_tx_frame, 0, 10);
    memset((uint8_t*)g_tx_last_frame, 0, 10);
    g_tx_frame_count = 0;
}

/**
 * @brief  发送一帧数据
 * @note   组帧格式: 0xAA 0x55 | 0x01 | 5 | 角速度1(u16 LE) | 保留(u16) | 红外状态(u8) | XOR
 */
void Protocol_SendFrame(void)
{
    uint8_t xor_sum = 0;
    uint16_t omega1_u16;

    /* 帧头 */
    g_tx_frame[0] = PROTOCOL_HEAD_1;
    g_tx_frame[1] = PROTOCOL_HEAD_2;

    /* 模块ID */
    g_tx_frame[2] = PROTOCOL_MODULE_ID;

    /* 载荷长度 */
    g_tx_frame[3] = PROTOCOL_PAYLOAD_LEN;

    /* 单霍尔只测非负速率；uint16_t mrad/s饱和编码，避免符号溢出。 */
    {
        float scaled = g_hall1_omega * 1000.0f;
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 65534.0f) scaled = 0.0f; /* 65535保留，不把异常伪装成高速 */
        omega1_u16 = (uint16_t)(scaled + 0.5f);
    }

    /* 小端序: 低字节在前 */
    g_tx_frame[4] = (uint8_t)(omega1_u16 & 0xFFU);
    g_tx_frame[5] = (uint8_t)((omega1_u16 >> 8) & 0xFFU);

    /* 保留字段 */
    g_tx_frame[6] = 0x00;
    g_tx_frame[7] = 0x00;

    /* 红外状态: 1=有鱼, 0=无鱼 */
    g_tx_frame[8] = g_ir_sensor.fish_arrived;

    /* 计算XOR校验 (覆盖 [2]~[8]) */
    xor_sum = g_tx_frame[2];
    xor_sum ^= g_tx_frame[3];
    xor_sum ^= g_tx_frame[4];
    xor_sum ^= g_tx_frame[5];
    xor_sum ^= g_tx_frame[6];
    xor_sum ^= g_tx_frame[7];
    xor_sum ^= g_tx_frame[8];
    g_tx_frame[9] = xor_sum;

    /* 保存帧副本 */
    for (uint8_t i = 0; i < 10; i++) {
        g_tx_last_frame[i] = g_tx_frame[i];
    }

    /* 标记发送状态 */
    s_tx_complete = 0;

    /* 启动UART发送 */
    HAL_UART_Transmit_IT(&huart1, (uint8_t*)g_tx_frame, PROTOCOL_TOTAL_LEN);
}

/**
 * @brief  发送完成回调 (在UART中断中调用)
 */
void Protocol_TxCpltCallback(void)
{
    s_tx_complete = 1;
    g_tx_ready = 1;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
