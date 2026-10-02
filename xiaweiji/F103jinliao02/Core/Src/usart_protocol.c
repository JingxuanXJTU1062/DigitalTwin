/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usart_protocol.c
  * @brief          : 串口协议驱动
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

/* USER CODE BEGIN Includes */
extern UART_HandleTypeDef huart1;
/* USER CODE END Includes */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/* Private variables ----------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile uint8_t g_tx_ready;
volatile uint32_t g_tx_frame_count;  /* 发送帧计数 */

/* 发送帧缓冲 (可watch查看发送内容) */
/* 帧格式: [0]0xAA [1]0x55 [2]ID [3]LEN [4-5]omega [6]hall [7-8]pressure [9]fish [10]0x00 [11]XOR */
volatile uint8_t g_tx_frame[12];

/* 最后一次发送的帧数据副本 (方便调试watch) */
volatile uint8_t g_tx_last_frame[12];
volatile uint8_t g_tx_send_flag;  /* 发送标志，上位后可观察 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void Protocol_BuildFrame(void);
static void Protocol_SendAsync(uint8_t *data, uint16_t len);
/* USER CODE END PFP */

/* Private user code ----------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  协议初始化
  */
void Protocol_Init(void)
{
    g_tx_ready = 1;
    g_tx_send_flag = 0;
    memset((uint8_t*)g_tx_frame, 0, 12);
    memset((uint8_t*)g_tx_last_frame, 0, 12);
}

/**
  * @brief  组帧并发送
  */
void Protocol_SendFrame(void)
{
    if (g_tx_ready == 0) {
        return;
    }

    g_tx_frame_count++;  /* 发送计数+1 */
    Protocol_BuildFrame();
    
    /* 保存发送帧副本到last_frame，方便watch */
    for (uint8_t i = 0; i < 12; i++) {
        g_tx_last_frame[i] = g_tx_frame[i];
    }
    g_tx_send_flag = 1;
    
    Protocol_SendAsync((uint8_t *)g_tx_frame, 12);
    g_tx_ready = 0;
}

/**
  * @brief  发送完成回调
  */
void Protocol_TxCpltCallback(void)
{
    g_tx_ready = 1;
}

/**
  * @brief  构建发送帧
  * @note   帧格式:
  *         [0] 0xAA      - 帧头1
  *         [1] 0x55      - 帧头2
  *         [2] MODULE_ID - 模块ID (0x02)
  *         [3] LEN       - 数据长度 (7)
  *         [4] omega_L   - 角速度低字节 (rad/s × 1000，mrad/s)
  *         [5] omega_H   - 角速度高字节
  *         [6] pressure_L - 压力低字节 (kPa × 100)
  *         [7] pressure_H - 压力高字节
  *         [8] fish_drop - 鱼体检测 (1=有鱼落下)
  *         [9] 0x00    - 保留
  *         [10] 0x00    - 保留
  *         [11] XOR     - 校验和
  */
static void Protocol_BuildFrame(void)
{
    uint8_t xor_sum;
    uint16_t omega_u16;
    uint16_t pressure_u16;

    g_tx_frame[0] = 0xAA;
    g_tx_frame[1] = 0x55;
    g_tx_frame[2] = MODULE_ID;
    g_tx_frame[3] = FRAME_LENGTH;

    /* 单霍尔只测非负速率；uint16_t mrad/s饱和编码。 */
    {
        float scaled = g_hall1_omega * 1000.0f;
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 65534.0f) scaled = 0.0f; /* 65535保留，不把异常伪装成高速 */
        omega_u16 = (uint16_t)(scaled + 0.5f);
    }
    g_tx_frame[4] = (uint8_t)(omega_u16 & 0xFFU);
    g_tx_frame[5] = (uint8_t)((omega_u16 >> 8) & 0xFFU);

    /* 压力值 (kPa) -> uint16_t, 放大100倍保留两位小数 */
    pressure_u16 = (uint16_t)(g_pressure.pressure_kpa * 100.0f);
    g_tx_frame[6] = (uint8_t)(pressure_u16 & 0xFF);       /* 低字节 */
    g_tx_frame[7] = (uint8_t)((pressure_u16 >> 8) & 0xFF); /* 高字节 */

    /* 鱼体检测 */
    g_tx_frame[8] = g_pressure.fish_drop;

    /* 保留 */
    g_tx_frame[9] = 0x00;
    g_tx_frame[10] = 0x00;

    /* XOR校验: ID ^ LEN ^ data[0] ^ data[1] ^ ... ^ data[n] */
    xor_sum = g_tx_frame[2];
    xor_sum ^= g_tx_frame[3];
    xor_sum ^= g_tx_frame[4];
    xor_sum ^= g_tx_frame[5];
    xor_sum ^= g_tx_frame[6];
    xor_sum ^= g_tx_frame[7];
    xor_sum ^= g_tx_frame[8];
    xor_sum ^= g_tx_frame[9];
    xor_sum ^= g_tx_frame[10];
    g_tx_frame[11] = xor_sum;
}

/**
  * @brief  异步发送
  */
static void Protocol_SendAsync(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit_IT(&huart1, (uint8_t*)data, len);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
