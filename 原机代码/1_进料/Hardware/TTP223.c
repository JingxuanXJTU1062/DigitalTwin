#include "ttp223.h"

void TTP223_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(TTP223_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin = TTP223_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TTP223_GPIO_PORT, &GPIO_InitStructure);
}

uint8_t TTP223_ReadState(void)
{
    return GPIO_ReadInputDataBit(TTP223_GPIO_PORT, TTP223_GPIO_PIN);
}
