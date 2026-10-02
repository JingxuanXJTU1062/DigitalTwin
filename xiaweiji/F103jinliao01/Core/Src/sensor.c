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

/*========================== 霍尔传感器变量 ==========================*/
/* 霍尔初始化完成标志 */
volatile uint32_t g_hall_init_ok = 0U;

/* 霍尔捕获中断计数 */
volatile uint32_t g_hall_capture_irq_count = 0U;

/* 霍尔轮询计数 */
volatile uint32_t g_hall_poll_count = 0U;

/* 霍尔通道数据 */
volatile HallChannel_TypeDef g_hall_channel;

/* 角速度输出 (供协议层使用) */
volatile float g_hall1_omega = 0.0f;  /* rad/s */

/* 内部变量 - 中断中复制的快照 */
static HallChannel_TypeDef s_hall_snapshot;
/* TIM3 is 16-bit; update interrupts extend it to a 32-bit 1 us timestamp. */
static volatile uint32_t s_tim3_overflow_count = 0U;

/*========================== 红外传感器 ==========================*/
volatile IrSensor_TypeDef g_ir_sensor;

/* 红外消抖计数 */
static uint8_t s_ir_debounce_count = 0U;
static uint8_t s_ir_last_state = 0U;
#define IR_DEBOUNCE_THRESHOLD   3U  /* 连续3次检测到稳定状态才确认 */

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
    uint64_t adaptive_ms = ((uint64_t)period_us * HALL_TIMEOUT_PERIOD_MULTIPLIER) / 1000U;
    if (adaptive_ms < HALL_BASE_TIMEOUT_MS) {
        return HALL_BASE_TIMEOUT_MS;
    }
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
    if (elapsed_ms == 0U) {
        return omega_mrad;
    }
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

    /* 复位红外传感器 */
    g_ir_sensor.raw_level = 0U;
    g_ir_sensor.fish_arrived = 0U;
    s_ir_debounce_count = 0U;
    s_ir_last_state = 0U;
}

/**
 * @brief  启动霍尔传感器 (在main.c中调用)
 */
void Hall_Start(void)
{
    /* Start update IRQ first so 16-bit counter overflows are counted. */
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
    /* Resolve capture/update IRQ ordering when both flags are pending. */
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
    if ((trusted_period > 0U) &&
        (elapsed_since_edge >= hall_get_timeout_ms(trusted_period))) {
        s->period_count = 0U; s->period_next = 0U;
        s->last_capture = capture; s->last_edge_tick = now_ms; s->glitch = 0U;
        return;
    }

    /* Unsigned subtraction naturally handles the extended timestamp wrap. */
    uint32_t delta = capture - s->last_capture;

    /* 毛刺过滤: delta太小认为是毛刺 */
    if (delta < HALL_MIN_VALID_PERIOD_US) {
        s->glitch = 1;
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

/**
 * @brief  更新霍尔传感器 (主循环调用)
 * @note   计算角速度
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

    /* 已有可信周期时自适应；首次捕获仍保留20s慢速识别窗口。 */
    uint32_t elapsed = now_ms - s->last_edge_tick;
    uint32_t timeout_ms = (period_used > 0U) ? hall_get_timeout_ms(period_used)
                                             : HALL_REACQUIRE_TIMEOUT_MS;
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
 * @brief  更新红外传感器数据 (带消抖)
 */
void Sensor_UpdateIr(void)
{
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(IR_SENSOR_PORT, IR_SENSOR_PIN);

    /* 保存原始电平: 1=HIGH, 0=LOW */
    uint8_t current_level = (pin_state == GPIO_PIN_SET) ? 1 : 0;
    g_ir_sensor.raw_level = current_level;

    /* 消抖逻辑 */
    if (current_level == s_ir_last_state) {
        if (s_ir_debounce_count < IR_DEBOUNCE_THRESHOLD) {
            s_ir_debounce_count++;
        }
    } else {
        s_ir_debounce_count = 0U;
        s_ir_last_state = current_level;
    }

    /* 消抖完成后确认状态 */
    if (s_ir_debounce_count >= IR_DEBOUNCE_THRESHOLD) {
        /* NPN开漏输出: 遮挡=低电平 -> 转换为鱼体到达标志 */
        /* 1=有鱼(LOW), 0=无鱼(HIGH) */
        g_ir_sensor.fish_arrived = (current_level == 0) ? 1 : 0;
    }
}

/**
 * @brief  传感器统一更新 (供main.c调用)
 */
void Sensor_Update(void)
{
    Sensor_UpdateHall();
    Sensor_UpdateIr();
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
