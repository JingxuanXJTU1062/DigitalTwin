#ifndef __TTP223_H
#define __TTP223_H

#include "stm32f10x.h"

// 引脚定义
#define TTP223_GPIO_PORT         GPIOA
#define TTP223_GPIO_PIN          GPIO_Pin_6
#define TTP223_GPIO_CLK          RCC_APB2Periph_GPIOA

// 函数声明
void TTP223_Init(void);
uint8_t TTP223_ReadState(void);

#endif
