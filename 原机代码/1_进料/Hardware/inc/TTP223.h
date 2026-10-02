#ifndef __TTP223_H
#define __TTP223_H

#include "stm32f10x.h"

// 引脚定义
#define TTP223_GPIO_PORT         GPIOA
#define TTP223_GPIO_PIN          GPIO_Pin_6
#define TTP223_GPIO_CLK          RCC_APB2Periph_GPIOA
#define TTP223_AFIO_CLK          RCC_APB2Periph_AFIO

// 对应 EXTI 配置
#define TTP223_EXTI_LINE         EXTI_Line6
#define TTP223_PORT_SOURCE       GPIO_PortSourceGPIOA
#define TTP223_PIN_SOURCE        GPIO_PinSource6
#define TTP223_IRQn              EXTI9_5_IRQn

// 函数声明
void TTP223_Init(void);
uint8_t TTP223_ReadState(void);
uint8_t TTP223_GetTouchFlag(void);
void TTP223_ClearTouchFlag(void);

#endif
