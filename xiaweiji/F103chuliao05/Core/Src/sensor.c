/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : sensor.c
  * @brief          : 传感器数据处理
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "sensor.h"
#include "main.h"
#include "string.h"

extern TIM_HandleTypeDef htim3;
extern ADC_HandleTypeDef hadc1;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 角速度计算常数: 2π × 10^6 */
#define OMEGA_CONST             6283185U

/* USER CODE END PD */

/* Private macro ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/*========================== 霍尔传感器变量 (输入捕获方式) ==========================*/
/* 霍尔初始化完成标志 */
volatile uint32_t g_hall_init_ok = 0U;

/* 霍尔捕获中断计数 */
volatile uint32_t g_hall_capture_irq_count = 0U;

/* 霍尔轮询计数 */
volatile uint32_t g_hall_poll_count = 0U;

/* 霍尔通道数据 */
volatile HallChannel_TypeDef g_hall_channel;

/* 角速度输出 (mrad/s，供协议层使用) */
volatile float g_hall1_omega = 0.0f;

/* 内部变量 - 中断中复制的快照 */
static HallChannel_TypeDef s_hall_snapshot;
/* TIM3为16位定时器，用更新中断扩展为32位1us时间戳 */
static volatile uint32_t s_tim3_overflow_count = 0U;

/*========================== 压力传感器 ==========================*/
volatile PressureSensor_TypeDef g_pressure;

/* 压力传感器 - 去抖动用 */
static uint8_t s_pressure_key_up = 1;      /* DO松开标志 */
static uint8_t s_pressure_debounce_cnt = 0; /* 去抖计数器 */

/* 压力传感器 - ADC滤波用 */
static uint16_t s_adc_samples[10];         /* ADC采样缓冲 */
static uint8_t s_adc_index = 0;            /* 当前采样索引 */
static uint32_t s_adc_sum = 0;             /* 采样累加和 */
static uint8_t s_adc_valid_count = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static uint32_t calc_omega(uint32_t period_us);
static uint32_t hall_period_from_samples(const volatile HallChannel_TypeDef *s);
static uint32_t hall_get_timeout_ms(uint32_t period_us);
static uint32_t hall_apply_waiting_limit(uint32_t omega_mrad, uint32_t elapsed_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*========================== 传感器函数实现 ==========================*/

/**
 * @brief  计算角速度
 */
static uint32_t calc_omega(uint32_t period_us)
{
    /* period_us 是相邻 IC 边沿的间隔 (单位: µs, 因为 TIM3 1MHz 计数) */
    if (period_us == 0U || period_us > (HALL_REACQUIRE_TIMEOUT_MS * 1000U)) {
        return 0U;
    }
    /* omega_rad_s = 2π / (period_s)
     *            = 2π × 10^6 / (period_us × EDGES_PER_REV)
     * omega_mrad_s = omega_rad_s × 1000
     *               = 2π × 10^9 / (period_us × EDGES_PER_REV)
     */
    uint64_t result = (uint64_t)OMEGA_CONST * 1000U;
    result /= (uint64_t)period_us * HALL_EDGES_PER_REV;
    if (result > 0xFFFFFFFFU) {
        result = 0xFFFFFFFFU;
    }
    return (uint32_t)result;
}

/**
 * @brief  获取超时阈值(ms)
 */
static uint32_t hall_get_timeout_ms(uint32_t period_us)
{
    uint64_t adaptive_ms = ((uint64_t)period_us * HALL_TIMEOUT_PERIOD_NUMERATOR) /
                           (1000U * HALL_TIMEOUT_PERIOD_DENOMINATOR);
    if (adaptive_ms < HALL_BASE_TIMEOUT_MS) return HALL_BASE_TIMEOUT_MS;
    if (adaptive_ms > HALL_TIMEOUT_MAX_MS) return HALL_TIMEOUT_MAX_MS;
    return (adaptive_ms > 0xFFFFFFFFU) ? 0xFFFFFFFFU : (uint32_t)adaptive_ms;
}

static uint32_t hall_period_from_samples(const volatile HallChannel_TypeDef *s)
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

static uint32_t hall_apply_waiting_limit(uint32_t omega_mrad, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0U) return omega_mrad;
    uint64_t upper = (uint64_t)OMEGA_CONST * 1000U;
    upper /= (uint64_t)elapsed_ms * 1000U * HALL_EDGES_PER_REV;
    return (upper < omega_mrad) ? (uint32_t)upper : omega_mrad;
}

/**
  * @brief  传感器变量初始化
  */
void Sensor_Init(void)
{
    /* 复位霍尔传感器 */
    memset((void*)&g_hall_channel, 0, sizeof(HallChannel_TypeDef));
    g_hall_init_ok = 0U;
    g_hall_capture_irq_count = 0U;
    g_hall_poll_count = 0U;
    g_hall1_omega = 0.0f;
    s_tim3_overflow_count = 0U;

    /* 复位压力传感器 */
    g_pressure.raw_level = 0;
    g_pressure.fish_drop = 0;
    g_pressure.adc_value = 0;
    g_pressure.pressure_kpa = 0.0f;
    memset(s_adc_samples, 0, sizeof(s_adc_samples));
    s_adc_index = 0;
    s_adc_sum = 0;
    s_adc_valid_count = 0U;
}

/**
  * @brief  启动霍尔传感器 (在main.c中调用)
  */
void Hall_Start(void)
{
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    g_hall_init_ok = 1U;
}

/**
  * @brief  TIM3输入捕获中断回调
  * @note   由HAL_TIM_IRQHandler调用
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3) {
        return;
    }

    /* 读取捕获值 */
    uint32_t capture16 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    uint32_t overflow = s_tim3_overflow_count;
    if ((__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE) != RESET) && capture16 < 0x8000U) {
        overflow++;
    }
    uint32_t capture = (overflow << 16) | capture16;
    uint32_t now_ms = HAL_GetTick();

    /* 更新中断计数 */
    g_hall_capture_irq_count++;

    /* 获取通道状态 */
    volatile HallChannel_TypeDef *s = &g_hall_channel;

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

    uint32_t delta = capture - s->last_capture;

    /* 毛刺过滤: delta太小认为是毛刺 */
    if (delta < HALL_MIN_VALID_PERIOD_US) {
        s->glitch = 1;
        return;
    }
    if (delta > (HALL_REACQUIRE_TIMEOUT_MS * 1000U)) {
        s->period_count = 0U; s->period_next = 0U;
        s->last_capture = capture; s->last_edge_tick = now_ms; s->glitch = 0U;
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

/**
  * @brief  更新霍尔传感器 (主循环调用)
  * @note   计算角速度，使用输入捕获模式
  */
void Sensor_UpdateHall(void)
{
    g_hall_poll_count++;

    uint32_t now_ms = HAL_GetTick();

    /* 临界区保护: 复制通道数据 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_hall_snapshot = g_hall_channel;
    __set_PRIMASK(primask);

    HallChannel_TypeDef *s = &s_hall_snapshot;
    uint32_t omega_mrad = 0U;
    uint8_t status = 0U;
    uint32_t period_used = 0U;

    period_used = hall_period_from_samples(s);
    uint32_t elapsed = now_ms - s->last_edge_tick;
    uint32_t timeout_ms = (period_used > 0U) ? hall_get_timeout_ms(period_used) : HALL_REACQUIRE_TIMEOUT_MS;
    if (s->seeded && elapsed >= timeout_ms) {
        status |= HALL_STATUS_TIMEOUT;
        omega_mrad = 0U;
    }
    /* 检查毛刺 */
    else if (s->glitch) {
        status |= HALL_STATUS_GLITCH;
        status |= HALL_STATUS_VALID;
        omega_mrad = s->omega_mrad;
    }
    /* 计算角速度(中值滤波) */
    else if (s->period_count > 0) {
        omega_mrad = calc_omega(period_used);
        omega_mrad = hall_apply_waiting_limit(omega_mrad, elapsed);
        status |= HALL_STATUS_VALID;
    }

    /* 存储结果 */
    g_hall_channel.omega_mrad = omega_mrad;
    g_hall_channel.status = status;

    /* 转换为rad/s供协议层使用 */
    g_hall1_omega = (float)omega_mrad / 1000.0f;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        s_tim3_overflow_count++;
    }
}

/**
  * @brief  更新压力传感器数据 (DO数字量)
  * @note   参照例程: 下拉输入，有去抖动，低电平=有鱼落下
  */
void Sensor_UpdatePressure(void)
{
    GPIO_PinState state;

    state = HAL_GPIO_ReadPin(PRESSURE_DO_PORT, PRESSURE_DO_PIN);

    /* 参照例程FSR_Scan逻辑，有去抖动 */
    if (s_pressure_key_up && state == GPIO_PIN_RESET) {
        /* 检测到低电平，启动去抖 */
        s_pressure_debounce_cnt++;
        if (s_pressure_debounce_cnt >= 2) {  /* 连续2次检测到低电平 */
            s_pressure_debounce_cnt = 0;
            s_pressure_key_up = 0;
            g_pressure.raw_level = 0;
            g_pressure.fish_drop = 1;  /* 有鱼落下 */
        }
    } else if (state == GPIO_PIN_SET) {
        /* 高电平，松开 */
        s_pressure_debounce_cnt = 0;
        s_pressure_key_up = 1;
        g_pressure.raw_level = 1;
        g_pressure.fish_drop = 0;  /* 无鱼 */
    } else {
        /* 低电平但还在去抖过程中 */
        s_pressure_debounce_cnt++;
        if (s_pressure_debounce_cnt >= 2) {
            s_pressure_debounce_cnt = 0;
        }
    }
}

/**
  * @brief  读取ADC并转换压力值
  * @note   参照例程: 软件触发，10次平均滤波
  */
void Sensor_UpdatePressureADC(void)
{
    uint32_t adc_raw;
    uint32_t adc_avg;

    /* 软件触发ADC转换 (单次模式) */
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_raw = HAL_ADC_GetValue(&hadc1);

        /* 10次平均滤波；启动阶段只除以实际有效样本数。 */
        if (s_adc_valid_count == 10U) {
            s_adc_sum -= s_adc_samples[s_adc_index];
        } else {
            s_adc_valid_count++;
        }
        s_adc_samples[s_adc_index] = (uint16_t)adc_raw;
        s_adc_sum += adc_raw;
        s_adc_index = (s_adc_index + 1) % 10;

        /* 计算平均值 */
        adc_avg = s_adc_sum / s_adc_valid_count;
        g_pressure.adc_value = (uint16_t)adc_avg;

        /* ADC 0-4095（0-3.3V）线性映射为0-100kPa。 */
        g_pressure.pressure_kpa = (float)adc_avg * PRESSURE_FULL_SCALE_KPA / 4095.0f;
    }

    HAL_ADC_Stop(&hadc1);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
