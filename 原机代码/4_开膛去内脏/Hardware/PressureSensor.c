#include "PressureSensor.h"
#include "delay.h"

static void PressureSensor_ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET);

    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET);
}

void PressureSensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(PRESSURE_SENSOR_ADC_CLK | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = PRESSURE_SENSOR_ADC_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PRESSURE_SENSOR_ADC_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PRESSURE_SENSOR_DO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(PRESSURE_SENSOR_DO_PORT, &GPIO_InitStructure);

    PressureSensor_ADC_Config();
}

uint16_t PressureSensor_ReadRaw(void)
{
    ADC_RegularChannelConfig(ADC1, PRESSURE_SENSOR_ADC_CHANNEL, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);
}

uint16_t PressureSensor_ReadRawAverage(uint8_t samples)
{
    uint32_t sum = 0;
    uint8_t i;

    if(samples == 0)
    {
        samples = 1;
    }

    for(i = 0; i < samples; i++)
    {
        sum += PressureSensor_ReadRaw();
        delay_ms(5);
    }

    return (uint16_t)(sum / samples);
}

float PressureSensor_ReadVoltage(void)
{
    uint16_t adc = PressureSensor_ReadRawAverage(10);
    return ((float)adc * 3.3f) / 4095.0f;
}

uint8_t PressureSensor_ReadDO(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(PRESSURE_SENSOR_DO_PORT, PRESSURE_SENSOR_DO_PIN);
}
