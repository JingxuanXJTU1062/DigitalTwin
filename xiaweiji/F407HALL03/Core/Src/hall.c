/**
 ******************************************************************************
 * @file    hall.c
 * @brief   霍尔传感器驱动实现 - 2路TIM输入捕获 + 角速度计算
 *
 * 功能:
 * - 2路霍尔传感器通过TIM2输入捕获测量
 * - CH1: PA0, CH4: PA3
 * - 计算角速度 (rad/s) 和转速 (RPM)
 * - 毛刺过滤和超时检测
 *
 * 角速度计算:
 * - omega = 2π × f = 2π / T
 * - T = 周期 (秒) = period_us × 10^-6
 * - omega_mrad = 2π × 10^3 / (period_us × pulses_per_rev)
 * - 常数: 6283185 / (period_us × pulses_per_rev)
 ******************************************************************************
 */

#include "hall.h"
#include "tim.h"
#include "main.h"

/*============================================================================
 * 常量定义
 *============================================================================*/

/** @brief 每转脉冲数 (默认1脉冲/转，需根据实际磁钢数调整) */
#ifndef HALL_PULSES_PER_REV
#define HALL_PULSES_PER_REV     1U
#endif

/** @brief 角速度计算常数: 2π × 10^6 */
#define OMEGA_CONST             6283185U

/*============================================================================
 * 全局变量定义
 *============================================================================*/

/** @brief 霍尔通道数组 [0]=CH1(PA0), [1]=CH4(PA3) */
volatile HallChannel g_hall_channel[HALL_CHANNEL_COUNT];

/** @brief 霍尔初始化完成标志 */
volatile uint32_t g_hall_init_ok = 0U;

/** @brief 霍尔轮询计数 */
volatile uint32_t g_hall_poll_count = 0U;

/** @brief 各通道输入捕获中断计数 */
volatile uint32_t g_hall_capture_irq_count[HALL_CHANNEL_COUNT] = {0U, 0U};

/** @brief 毛刺过滤阈值 (μs) */
uint32_t g_hall_glitch_threshold = HALL_MIN_VALID_PERIOD_US;

/*============================================================================
 * 内部变量
 *============================================================================*/

/** @brief 临时快照 (中断中复制数据用) */
static HallChannel s_hall_snapshot[HALL_CHANNEL_COUNT];

/*============================================================================
 * 内部函数
 *============================================================================*/

/**
 * @brief   计算通道角速度
 * @param   ch      通道号
 * @param   period  周期 (μs)
 * @retval  角速度 (mrad/s)
 */
static uint32_t calc_omega(uint32_t period)
{
    if (period == 0U || period > (HALL_REACQUIRE_TIMEOUT_MS * 1000U)) {
        return 0U;  /* 无效周期 */
    }

    /* omega_mrad_s = 2π × 10^9 / (period_us × EDGES_PER_REV)
     * 与 F103jinliao01/jinliao02/chuliao05 公式一致 (传感器与协议说明.md 第 250 行) */
    uint64_t result = (uint64_t)OMEGA_CONST * 1000U;
    result /= (uint64_t)period * HALL_PULSES_PER_REV;
    if (result > 0xFFFFFFFFU) {
        result = 0xFFFFFFFFU;
    }
    return (uint32_t)result;
}

static uint32_t hall_period_from_samples(const volatile HallChannel *s)
{
    if (s->period_count == 0U) return 0U;
    if (s->period_count == 1U) return s->period_buf[0];
    if (s->period_count == 2U) return (s->period_buf[0] + s->period_buf[1]) / 2U;
    uint32_t p0 = s->period_buf[0], p1 = s->period_buf[1], p2 = s->period_buf[2];
    if (p0 > p1) { uint32_t t = p0; p0 = p1; p1 = t; }
    if (p1 > p2) { uint32_t t = p1; p1 = p2; p2 = t; }
    if (p0 > p1) { uint32_t t = p0; p0 = p1; p1 = t; }
    return p1;
}

/**
 * @brief   获取超时阈值 (ms)
 */
static uint32_t hall_get_timeout_ms(uint32_t period_us)
{
    uint64_t adaptive_ms = ((uint64_t)period_us * HALL_TIMEOUT_PERIOD_NUMERATOR) /
                           (1000U * HALL_TIMEOUT_PERIOD_DENOMINATOR);
    if (adaptive_ms < HALL_BASE_TIMEOUT_MS) return HALL_BASE_TIMEOUT_MS;
    if (adaptive_ms > HALL_TIMEOUT_MAX_MS) return HALL_TIMEOUT_MAX_MS;
    return (adaptive_ms > 0xFFFFFFFFU) ? 0xFFFFFFFFU : (uint32_t)adaptive_ms;
}

static uint32_t hall_apply_waiting_limit(uint32_t omega_mrad, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0U) return omega_mrad;
    uint64_t upper = (uint64_t)OMEGA_CONST * 1000U;
    upper /= (uint64_t)elapsed_ms * 1000U * HALL_PULSES_PER_REV;
    return (upper < omega_mrad) ? (uint32_t)upper : omega_mrad;
}

/**
 * @brief   计算RPM
 * @param   period_us  周期 (μs)
 * @retval  RPM
 */
static float calc_rpm(uint32_t period_us)
{
    if (period_us == 0) {
        return 0.0f;
    }
    /* RPM = 60 × 10^6 / (period_us × pulses_per_rev) */
    float rpm = 60000000.0f / ((float)period_us * HALL_PULSES_PER_REV);
    return rpm;
}

/*============================================================================
 * 输入捕获中断回调
 *============================================================================*/

/**
 * @brief   TIM输入捕获回调
 * @note    在TIM2_IRQHandler中调用
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2) {
        return;  /* 只处理TIM2 */
    }
    
    uint8_t ch = 0;
    uint32_t channel;
    
    /* 确定是哪个通道 */
    switch (htim->Channel) {
        case HAL_TIM_ACTIVE_CHANNEL_1:
            ch = 0;
            channel = TIM_CHANNEL_1;
            break;
        case HAL_TIM_ACTIVE_CHANNEL_4:
            ch = 1;
            channel = TIM_CHANNEL_4;
            break;
        default:
            return;  /* 无效通道 */
    }
    
    /* 读取捕获值 */
    uint32_t capture = HAL_TIM_ReadCapturedValue(htim, channel);
    uint32_t now_ms = HAL_GetTick();
    
    /* 更新中断计数 */
    g_hall_capture_irq_count[ch]++;
    
    /* 获取通道状态 */
    volatile HallChannel *s = &g_hall_channel[ch];
    
    /* 超时只在Hall_Update中用于停转清零；收到的低速边沿不能被丢弃。 */
    if (!s->seeded) {
        s->seeded = 1;
        s->period_count = 0;
        s->period_next = 0;
        s->glitch = 0;
        s->last_capture = capture;
        s->last_edge_tick = now_ms;
        return;
    }

    uint32_t trusted_period = hall_period_from_samples(s);
    uint32_t elapsed_since_edge = now_ms - s->last_edge_tick;
    if ((trusted_period > 0U) && (elapsed_since_edge >= hall_get_timeout_ms(trusted_period))) {
        s->period_count = 0U; s->period_next = 0U;
        s->last_capture = capture; s->last_edge_tick = now_ms; s->glitch = 0U;
        return;
    }
    
    /* 计算周期增量 (考虑定时器溢出) */
    uint32_t delta;
    if (capture >= s->last_capture) {
        delta = capture - s->last_capture;
    } else {
        /* 定时器溢出: 0xFFFFFFFF + delta */
        delta = (0xFFFFFFFFU - s->last_capture) + capture + 1U;
    }
    
    /* 毛刺过滤: delta太小认为是毛刺 */
    if ((delta < HALL_MIN_VALID_PERIOD_US) || (delta < g_hall_glitch_threshold)) {
        s->glitch = 1;
        /* 不更新last_capture，保留有效边沿 */
        return;
    }
    if (delta > (HALL_REACQUIRE_TIMEOUT_MS * 1000U)) {
        s->period_count = 0U;
        s->period_next = 0U;
        s->last_capture = capture;
        s->last_edge_tick = now_ms;
        s->glitch = 0U;
        return;
    }
    
    /* 存储周期到环形缓冲区 */
    s->period_buf[s->period_next] = delta;
    s->period_next = (s->period_next + 1U) % 3U;
    if (s->period_count < 3U) {
        s->period_count++;
    }
    
    /* 更新边沿时间 */
    s->last_capture = capture;
    s->last_edge_tick = now_ms;
    s->glitch = 0;
}

/*============================================================================
 * 函数实现
 *============================================================================*/

/**
 * @brief   霍尔传感器初始化
 */
void Hall_Init(void)
{
    /* 初始化通道结构 */
    for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
        g_hall_channel[i].capture_count = 0U;
        g_hall_channel[i].last_capture = 0U;
        g_hall_channel[i].last_edge_tick = 0U;
        g_hall_channel[i].period_buf[0] = 0U;
        g_hall_channel[i].period_buf[1] = 0U;
        g_hall_channel[i].period_buf[2] = 0U;
        g_hall_channel[i].period_next = 0U;
        g_hall_channel[i].period_count = 0U;
        g_hall_channel[i].seeded = 0U;
        g_hall_channel[i].glitch = 0U;
        g_hall_channel[i].omega_mrad = 0U;
        g_hall_channel[i].status = 0U;
        g_hall_channel[i].rpm = 0.0f;
    }
    
    /* 初始化变量 */
    g_hall_init_ok = 0U;
    g_hall_poll_count = 0U;
    g_hall_capture_irq_count[0] = 0U;
    g_hall_capture_irq_count[1] = 0U;
    
    /* 启动TIM2输入捕获 (2路) */
    /* 注意: MX_TIM2_Init()已经在main.c中调用 */
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }
    
    g_hall_init_ok = 1U;
}

/**
 * @brief   霍尔数据更新
 */
void Hall_Update(void)
{
    g_hall_poll_count++;
    
    uint32_t now_ms = HAL_GetTick();
    
    /* 临界区保护: 复制通道数据 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
        s_hall_snapshot[i] = g_hall_channel[i];
    }
    __set_PRIMASK(primask);
    
    /* 处理每个通道 */
    for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
        HallChannel *s = &s_hall_snapshot[i];
        uint8_t ch_status = 0U;
        uint32_t ch_omega = 0U;
        uint32_t period_used = 0U;
        float ch_rpm = 0.0f;
        
        period_used = hall_period_from_samples(s);
        uint32_t elapsed = now_ms - s->last_edge_tick;
        uint32_t timeout_ms = (period_used > 0U) ? hall_get_timeout_ms(period_used) : HALL_REACQUIRE_TIMEOUT_MS;
        if (s->seeded && elapsed >= timeout_ms) {
            ch_status |= HALL_STATUS_TIMEOUT;
            ch_omega = 0U;
        }
        /* 检查毛刺 */
        else if (s->glitch) {
            ch_status |= HALL_STATUS_GLITCH;
            ch_status |= HALL_STATUS_VALID;  /* 保留之前有效值 */
            ch_omega = s->omega_mrad;
            ch_rpm = s->rpm;
        }
        /* 计算角速度 */
        else if (s->period_count > 0) {
            ch_omega = calc_omega(period_used);
            ch_omega = hall_apply_waiting_limit(ch_omega, elapsed);
            ch_rpm = calc_rpm(period_used);
            ch_status |= HALL_STATUS_VALID;
        }
        
        /* 存储结果到全局变量 */
        g_hall_channel[i].omega_mrad = ch_omega;
        g_hall_channel[i].status = ch_status;
        g_hall_channel[i].rpm = ch_rpm;
        
    }

    /* 此函数不再调用 Protocol_BuildFrame(). main.c 通过 Hall_GetOmega() 拿到
     * omega 后直接送入 DistanceTx_Send() 构成 14 字节帧 (MODULE_ID=0x03). */
}

/**
 * @brief   获取指定通道的角速度
 */
uint32_t Hall_GetOmega(uint8_t ch)
{
    if (ch >= HALL_CHANNEL_COUNT) {
        return 0U;
    }
    return g_hall_channel[ch].omega_mrad;
}

/**
 * @brief   获取指定通道的状态
 */
uint8_t Hall_GetStatus(uint8_t ch)
{
    if (ch >= HALL_CHANNEL_COUNT) {
        return 0U;
    }
    return g_hall_channel[ch].status;
}

/**
 * @brief   获取所有通道角速度数组
 */
void Hall_GetAllOmega(uint32_t omega[HALL_CHANNEL_COUNT])
{
    for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
        omega[i] = g_hall_channel[i].omega_mrad;
    }
}

/**
 * @brief   获取所有通道状态数组
 */
void Hall_GetAllStatus(uint8_t status[HALL_CHANNEL_COUNT])
{
    for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
        status[i] = g_hall_channel[i].status;
    }
}

/**
 * @brief   重置霍尔通道
 */
void Hall_Reset(uint8_t ch)
{
    if (ch == 255U) {
        /* 重置所有通道 */
        for (uint8_t i = 0; i < HALL_CHANNEL_COUNT; i++) {
            g_hall_channel[i].seeded = 0U;
            g_hall_channel[i].period_count = 0U;
            g_hall_channel[i].period_next = 0U;
            g_hall_channel[i].glitch = 0U;
            g_hall_channel[i].omega_mrad = 0U;
            g_hall_channel[i].status = 0U;
        }
    } else if (ch < HALL_CHANNEL_COUNT) {
        g_hall_channel[ch].seeded = 0U;
        g_hall_channel[ch].period_count = 0U;
        g_hall_channel[ch].period_next = 0U;
        g_hall_channel[ch].glitch = 0U;
        g_hall_channel[ch].omega_mrad = 0U;
        g_hall_channel[ch].status = 0U;
    }
}

/**
 * @brief   设置毛刺过滤阈值
 */
void Hall_SetGlitchThreshold(uint32_t threshold_us)
{
    g_hall_glitch_threshold = threshold_us;
}
