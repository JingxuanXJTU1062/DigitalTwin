/**
 ******************************************************************************
 * @file    protocol.c
 * @brief   统一串口协议实现 - 按AGENTS.md规范封装霍尔数据帧
 *
 * 帧格式: [AA][55][module_id][len][data...][xor]
 * 数据区: [omega1_L][omega1_H][omega2_L][omega2_H][omega3_L][omega3_H][status12][status3]
 *
 * @note    omega单位: rad/s × 1000 (即毫弧度/秒)，范围0~60000
 *          状态字节: bit0=有效, bit1=超时, bit2=毛刺
 ******************************************************************************
 */

#include "protocol.h"
#include "usart.h"
#include "main.h"

/*============================================================================
 * 全局变量定义
 *============================================================================*/

/** @brief 发送就绪标志 */
volatile uint8_t g_tx_ready = 0U;

/** @brief 待发送的协议帧缓冲区 (13字节) */
uint8_t g_tx_frame[FRAME_TOTAL_LENGTH];

/** @brief 成功发送帧计数 */
volatile uint32_t g_tx_frame_count = 0U;

/** @brief 最后一次发送的系统节拍 (ms) */
volatile uint32_t g_tx_last_tick_ms = 0U;

/** @brief 最后一次发送状态 (0=成功, 非0=失败) */
volatile uint8_t g_tx_last_status = 0U;

/** @brief 发送尝试计数 */
volatile uint32_t g_tx_attempt_count = 0U;

/** @brief 串口忙计数 (调试用) */
volatile uint32_t g_tx_busy_count = 0U;

/*============================================================================
 * 内部变量
 *============================================================================*/

/** @brief 上次发送时间 (用于计算周期) */
static uint32_t s_last_send_tick = 0U;

/*============================================================================
 * 函数实现
 *============================================================================*/

/**
 * @brief   协议层初始化
 */
void Protocol_Init(void)
{
    g_tx_ready = 0U;
    g_tx_frame_count = 0U;
    g_tx_last_tick_ms = 0U;
    g_tx_last_status = 0U;
    g_tx_attempt_count = 0U;
    g_tx_busy_count = 0U;
    s_last_send_tick = 0U;
    
    /* 清零帧缓冲区 */
    for (uint8_t i = 0; i < FRAME_TOTAL_LENGTH; i++) {
        g_tx_frame[i] = 0U;
    }
}

/**
 * @brief   构建霍尔数据帧
 * @param   omega    角速度数组 (rad/s × 1000，uint32_t)
 * @param   status   状态数组 (bit0=有效, bit1=超时, bit2=毛刺)
 * @retval  none
 */
void Protocol_BuildFrame(const uint32_t omega[HALL_CHANNEL_COUNT],
                        const uint8_t status[HALL_CHANNEL_COUNT])
{
    uint32_t omega0_u16 = (omega[0] > 65535U) ? 65535U : omega[0];
    uint32_t omega1_u16 = (omega[1] > 65535U) ? 65535U : omega[1];
    uint32_t omega2_u16 = (omega[2] > 65535U) ? 65535U : omega[2];
    /* 帧头 [AA][55] */
    g_tx_frame[0] = FRAME_HEADER1;
    g_tx_frame[1] = FRAME_HEADER2;
    
    /* 模块ID */
    g_tx_frame[2] = MODULE_ID_HALL;
    
    /* 数据长度 */
    g_tx_frame[3] = HALL_DATA_LENGTH;
    
    /* 数据载荷 (8字节) */
    /* omega1: 2字节 (uint16_t, rad/s×1000) */
    g_tx_frame[4] = (uint8_t)(omega0_u16 & 0xFFU);
    g_tx_frame[5] = (uint8_t)((omega0_u16 >> 8) & 0xFFU);

    /* omega2: 2字节 */
    g_tx_frame[6] = (uint8_t)(omega1_u16 & 0xFFU);
    g_tx_frame[7] = (uint8_t)((omega1_u16 >> 8) & 0xFFU);

    /* omega3: 2字节 */
    g_tx_frame[8] = (uint8_t)(omega2_u16 & 0xFFU);
    g_tx_frame[9] = (uint8_t)((omega2_u16 >> 8) & 0xFFU);

    /* 状态字节: 每路使用3bit，第三路放在第二个状态字节 */
    g_tx_frame[10] = (status[0] & 0x07U) | ((status[1] & 0x07U) << 3);
    g_tx_frame[11] = status[2] & 0x07U;

    /* XOR校验和: 从module_id到data的所有字节异或 */
    uint8_t xor_sum = Protocol_CalcXor(&g_tx_frame[2], 2U + HALL_DATA_LENGTH);
    g_tx_frame[12] = xor_sum;
    
    /* 设置发送就绪标志 */
    g_tx_ready = 1U;
}

/**
 * @brief   发送帧
 * @retval  0=成功, 1=串口忙, 2=发送失败
 */
uint8_t Protocol_SendFrame(void)
{
    if (!g_tx_ready) {
        return 2U;  /* 无帧可发 */
    }
    
    /* 检查串口状态 */
    if (huart1.gState != HAL_UART_STATE_READY) {
        g_tx_busy_count++;
        return 1U;  /* 串口忙 */
    }
    
    /* 发送帧 */
    g_tx_attempt_count++;
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, g_tx_frame, FRAME_TOTAL_LENGTH, 20U);
    
    g_tx_last_tick_ms = HAL_GetTick();
    
    if (status == HAL_OK) {
        g_tx_last_status = 0U;
        g_tx_frame_count++;
        g_tx_ready = 0U;
        return 0U;
    } else {
        g_tx_last_status = (uint8_t)status;
        return 2U;
    }
}

/**
 * @brief   协议周期处理 (应在主循环调用)
 */
void Protocol_Poll(void)
{
    uint32_t now = HAL_GetTick();
    
    /* 检查是否到发送周期 */
    if ((uint32_t)(now - s_last_send_tick) < HALL_TX_PERIOD_MS) {
        return;
    }
    
    /* 尝试发送 */
    uint8_t ret = Protocol_SendFrame();
    
    /* 成功发送后更新计时 */
    if (ret == 0U) {
        s_last_send_tick = now;
    }
}
