/**
 ******************************************************************************
 * @file    protocol.h
 * @brief   统一串口协议 - 按AGENTS.md规范封装霍尔数据帧
 *
 * 帧格式: [AA][55][module_id][len][data...][xor]
 * 模块ID: 0x03 (F407HALL 串口上报)
 * 数据长度: 8字节 (3路×2字节omega + 2字节状态)
 *
 * @note    注意: F407HALL 当前使用 distance_tx.c 的 14 字节帧 (0x03)，
 *          protocol.c 仅作备份/扩展用，此处 MODULE_ID 与 distance_tx.h 保持一致
 ******************************************************************************
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "hall.h"

/* 模块ID定义 - 0x03 为 F407HALL (与 distance_tx.h 保持一致) */
#define MODULE_ID_HALL          0x03U

/* 帧头定义 */
#define FRAME_HEADER1           0xAAU
#define FRAME_HEADER2           0x55U

/* 数据长度定义 */
#define HALL_DATA_LENGTH        8U  /* 3路×2字节omega + 2字节状态 */
#define FRAME_TOTAL_LENGTH      (5U + HALL_DATA_LENGTH)  /* 13字节总长度 */

/* 注意: HALL_CHANNEL_COUNT 由 hall.h 定义（本文件包含 main.h 时已间接包含）*/

/* 发送周期 (ms) */
#define HALL_TX_PERIOD_MS       50U

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 全局变量 (在protocol.c中定义)
 *============================================================================*/

/** @brief 发送就绪标志 - 主循环检测此标志后执行发送 */
extern volatile uint8_t g_tx_ready;

/** @brief 待发送的协议帧缓冲区 (13字节) */
extern uint8_t g_tx_frame[FRAME_TOTAL_LENGTH];

/** @brief 成功发送帧计数 */
extern volatile uint32_t g_tx_frame_count;

/** @brief 最后一次发送的系统节拍 (ms) */
extern volatile uint32_t g_tx_last_tick_ms;

/** @brief 最后一次发送状态 (0=成功, 非0=失败) */
extern volatile uint8_t g_tx_last_status;

/** @brief 发送尝试计数 */
extern volatile uint32_t g_tx_attempt_count;

/** @brief 串口忙标志 (调试用) */
extern volatile uint32_t g_tx_busy_count;

/*============================================================================
 * 函数声明
 *============================================================================*/

/**
 * @brief   协议层初始化
 * @retval  none
 * @note    必须在系统初始化完成后调用
 */
void Protocol_Init(void);

/**
 * @brief   构建霍尔数据帧
 * @param   omega    角速度数组 (rad/s × 1000，uint32_t)
 * @param   status   状态数组 (bit0=有效, bit1=超时, bit2=毛刺)
 * @retval  none
 * @note    组装好的帧存入g_tx_frame，设置g_tx_ready=1
 */
void Protocol_BuildFrame(const uint32_t omega[HALL_CHANNEL_COUNT],
                        const uint8_t status[HALL_CHANNEL_COUNT]);

/**
 * @brief   发送帧
 * @retval  0=成功, 1=串口忙, 2=发送失败
 * @note    阻塞式发送约1.1ms @115200
 */
uint8_t Protocol_SendFrame(void);

/**
 * @brief   协议周期处理 (应在主循环调用)
 * @retval  none
 * @note    按HALL_TX_PERIOD_MS周期构建并发送帧
 */
void Protocol_Poll(void);

/**
 * @brief   计算XOR校验和
 * @param   data     数据指针
 * @param   len      数据长度
 * @retval  XOR校验结果
 */
static inline uint8_t Protocol_CalcXor(const uint8_t *data, uint8_t len)
{
    uint8_t xor_sum = 0;
    while (len--) {
        xor_sum ^= *data++;
    }
    return xor_sum;
}

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
