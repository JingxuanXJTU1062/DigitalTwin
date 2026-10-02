#ifndef __PRESSURE_SENSOR_H
#define __PRESSURE_SENSOR_H

#include "stm32f10x.h"

/*
 * 引脚分配（STM32F103C8T6）
 * AO -> PB0  (ADC1_IN8)
 * DO -> PB1  (数字输入)
 * VCC -> 3.3V
 * GND -> GND
 *
 * 注意：为保护 STM32 ADC，建议模块使用 3.3V 供电，确保 AO 不超过 3.3V。
 */

#define PRESSURE_SENSOR_ADC_PORT        GPIOB
#define PRESSURE_SENSOR_ADC_PIN         GPIO_Pin_0
#define PRESSURE_SENSOR_ADC_CLK         RCC_APB2Periph_GPIOB
#define PRESSURE_SENSOR_ADC_CHANNEL     ADC_Channel_8

#define PRESSURE_SENSOR_DO_PORT         GPIOB
#define PRESSURE_SENSOR_DO_PIN          GPIO_Pin_1
#define PRESSURE_SENSOR_DO_CLK          RCC_APB2Periph_GPIOB

void PressureSensor_Init(void);
uint16_t PressureSensor_ReadRaw(void);
uint16_t PressureSensor_ReadRawAverage(uint8_t samples);
float PressureSensor_ReadVoltage(void);
uint8_t PressureSensor_ReadDO(void);

#endif
