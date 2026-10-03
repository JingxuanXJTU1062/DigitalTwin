/**
 ******************************************************************************
 * @file    hall.h
 * @brief   霍尔传感器驱动 - 3路TIM输入捕获 + 角速度计算
 *
 * 支持3路霍尔传感器通过TIM2输入捕获测量转速
 * - CH1: PA0 (霍尔1)
 * - CH4: PA3 (霍尔2，保留原接线)
 * - CH3: PA2 (霍尔3)
 *
 * 角速度计算: omega = 2π / T (rad/s)，其中T为采样周期
 ******************************************************************************
 */

#ifndef HALL_H
#define HALL_H

#include <stdint.h>

/* 霍尔通道数 */
#define HALL_CHANNEL_COUNT      3U

/* 每转脉冲数 (需根据实际磁钢数修改) */
#define HALL_PULSES_PER_REV     1U

/* 角速度计算常数: omega = CONST / period_us / pulses_per_rev */
#define HALL_OMEGA_CONST        6283185U  /* 2π × 10^6 (period in μs, omega in mrad/s) */

/* 霍尔周期有效范围与自适应停转策略 */
#define HALL_BASE_TIMEOUT_MS             5000U
#define HALL_REACQUIRE_TIMEOUT_MS       10000U
#define HALL_MIN_VALID_PERIOD_US      1800000U
#define HALL_TIMEOUT_PERIOD_NUMERATOR       3U
#define HALL_TIMEOUT_PERIOD_DENOMINATOR     2U
#define HALL_TIMEOUT_MAX_MS             10000U

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 霍尔通道状态定义
 *============================================================================*/

/** @brief 霍尔通道状态位 */
#define HALL_STATUS_VALID       0x01U  /* bit0: 信号有效 */
#define HALL_STATUS_TIMEOUT    0x02U  /* bit1: 信号超时 */
#define HALL_STATUS_GLITCH     0x04U  /* bit2: 检测到毛刺 */

/*============================================================================
 * 霍尔数据结构
 *============================================================================*/

/**
 * @brief 霍尔通道数据结构
 */
typedef struct {
    /* 计数相关 */
    uint32_t capture_count;      /* 输入捕获中断计数 */
    uint32_t last_capture;      /* 上次捕获值 */
    uint32_t last_edge_tick;    /* 上次边沿时间 (ms) */
    
    /* 周期计算相关 */
    uint32_t period_buf[3];     /* 周期环形缓冲区 */
    uint8_t  period_next;       /* 下一个写入位置 */
    uint8_t  period_count;      /* 有效周期数 (0-3) */
    uint8_t  seeded;            /* 是否已播种 */
    uint8_t  glitch;            /* 毛刺标志 */
    
    /* 计算结果 */
    uint32_t omega_mrad;        /* 角速度 (mrad/s, 即rad/s×1000) */
    uint8_t  status;            /* 状态标志 */
    float    rpm;               /* 转速 (RPM, 仅调试用) */
} HallChannel;

/*============================================================================
 * 全局变量 (在hall.c中定义)
 *============================================================================*/

/** @brief 霍尔通道数组 [0]=CH1(PA0), [1]=CH4(PA3), [2]=CH3(PA2) */
extern volatile HallChannel g_hall_channel[HALL_CHANNEL_COUNT];

/** @brief 霍尔初始化完成标志 */
extern volatile uint32_t g_hall_init_ok;

/** @brief 霍尔轮询计数 */
extern volatile uint32_t g_hall_poll_count;

/** @brief 各通道输入捕获中断计数 */
extern volatile uint32_t g_hall_capture_irq_count[HALL_CHANNEL_COUNT];

/** @brief 毛刺过滤阈值 (最小周期μs) */
extern uint32_t g_hall_glitch_threshold;

/*============================================================================
 * 函数声明
 *============================================================================*/

/**
 * @brief   霍尔传感器初始化
 * @retval  none
 * @note    初始化TIM2输入捕获，启动3路通道
 */
void Hall_Init(void);

/**
 * @brief   霍尔数据更新 (应在主循环调用)
 * @retval  none
 * @note    更新各通道角速度，设置协议就绪标志
 */
void Hall_Update(void);

/**
 * @brief   获取指定通道的角速度
 * @param   ch      通道号 (0-2)
 * @retval  角速度 (mrad/s)
 */
uint32_t Hall_GetOmega(uint8_t ch);

/**
 * @brief   获取指定通道的状态
 * @param   ch      通道号 (0-2)
 * @retval  状态字节
 */
uint8_t Hall_GetStatus(uint8_t ch);

/**
 * @brief   获取所有通道角速度数组
 * @param   omega   输出数组指针 (必须为3元素)
 * @retval  none
 */
void Hall_GetAllOmega(uint32_t omega[HALL_CHANNEL_COUNT]);

/**
 * @brief   获取所有通道状态数组
 * @param   status  输出数组指针 (必须为3元素)
 * @retval  none
 */
void Hall_GetAllStatus(uint8_t status[HALL_CHANNEL_COUNT]);

/**
 * @brief   重置霍尔通道
 * @param   ch      通道号 (0-2)，255表示全部
 * @retval  none
 */
void Hall_Reset(uint8_t ch);

/**
 * @brief   设置毛刺过滤阈值
 * @param   threshold_us  阈值 (μs)
 * @retval  none
 * @note    小于此值的周期被判定为毛刺
 */
void Hall_SetGlitchThreshold(uint32_t threshold_us);

#ifdef __cplusplus
}
#endif

#endif /* HALL_H */
