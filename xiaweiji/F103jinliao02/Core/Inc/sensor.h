/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : sensor.h
  * @brief          : 传感器定义头文件
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SENSOR_H
#define __SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/*========================== 霍尔传感器参数 ==========================*/
/* 霍尔传感器3144E参数 */
#define HALL_PULSES_PER_REV     1U        /* 磁钢数量 */
#define HALL_EDGES_PER_REV      1U        /* 每转IC边沿数(单磁钢单极=1,双极=2) */
#define HALL_BASE_TIMEOUT_MS             5000U     /* 常规停转判定基准 */
#define HALL_REACQUIRE_TIMEOUT_MS       20000U     /* 首次/超慢周期捕获窗口 */
#define HALL_MIN_VALID_PERIOD_US      1800000U     /* 正常最短约2s，留10%裕量 */
#define HALL_TIMEOUT_PERIOD_MULTIPLIER      2U     /* 慢速时允许等待最近周期的2倍 */

/*========================== 模块参数 ==========================*/
#define MODULE_ID             0x02     /* 霍尔+压力模块ID */
#define FRAME_LENGTH          7        /* 数据载荷长度: omega(2) + pressure(2) + fish_drop(1) + reserved(2) */

/*========================== 传感器GPIO定义 ==========================*/
/* 霍尔传感器 - PA6 (TIM3_CH1), 由CubeMX自动配置在msp.c中 */

/* 压力传感器DO: PB0, 有鱼落下时DO拉低(低电平) */
#define PRESSURE_DO_PORT     GPIOB
#define PRESSURE_DO_PIN      GPIO_PIN_0

/* 压力传感器AO: PA1 (ADC通道1) */
#define PRESSURE_AO_PORT     GPIOA
#define PRESSURE_AO_PIN      GPIO_PIN_1

/* 线性显示映射：ADC 0-4095（0-3.3V）对应0-100kPa。 */
#define PRESSURE_FULL_SCALE_KPA             100.0f

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/**
 * @brief 霍尔通道数据结构
 */
typedef struct {
    uint32_t last_capture;      /* 上次扩展捕获时间戳(1us, 32位) */
    uint32_t last_edge_tick;    /* 上次边沿时间 (ms) */
    uint32_t period_buf[3];     /* 周期环形缓冲区 */
    uint8_t  period_next;       /* 下一个写入位置 */
    uint8_t  period_count;      /* 有效周期数 (0-3) */
    uint8_t  seeded;            /* 是否已播种 */
    uint8_t  glitch;           /* 毛刺标志 */
    uint32_t omega_mrad;        /* 角速度 (mrad/s) */
    uint8_t  status;            /* 状态标志 */
} HallChannel_TypeDef;

/* 霍尔通道状态位 */
#define HALL_STATUS_VALID       0x01U
#define HALL_STATUS_TIMEOUT    0x02U
#define HALL_STATUS_GLITCH     0x04U

/**
  * @brief 压力传感器数据结构
  */
typedef struct {
    volatile uint8_t raw_level;        /* 原始GPIO电平: 1=HIGH, 0=LOW */
    volatile uint8_t fish_drop;        /* 鱼体落盘: 1=有鱼(LOW), 0=无鱼(HIGH) */
    volatile uint16_t adc_value;       /* ADC原始值: 0-4095 (12位ADC) */
    volatile float pressure_kpa;        /* 压力值: kPa */
} PressureSensor_TypeDef;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported variables --------------------------------------------------------*/
/* 霍尔传感器变量 (输入捕获方式) */
extern volatile uint32_t g_hall_init_ok;            /* 初始化标志 */
extern volatile uint32_t g_hall_capture_irq_count;  /* 捕获中断计数 */
extern volatile uint32_t g_hall_poll_count;         /* 轮询计数 */
extern volatile HallChannel_TypeDef g_hall_channel;  /* 霍尔通道数据 */
extern volatile float g_hall1_omega;                /* 角速度 (rad/s)，供协议层使用 */

extern volatile PressureSensor_TypeDef g_pressure;  /* 压力传感器 */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
void Sensor_Init(void);
void Sensor_UpdateHall(void);
void Sensor_UpdatePressure(void);
void Sensor_UpdatePressureADC(void);
void Hall_Start(void);      /* 启动霍尔传感器TIM3输入捕获 */
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H */
