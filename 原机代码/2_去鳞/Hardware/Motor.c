#include "stm32f10x.h"                  // Device header
#include "PWM.h"

void Motor_Init(void)
{
    /*
     * 电机1方向: PA4, PA5
     * 电机2方向: PB10, PB11
     * 避开 PA6(TTP223) 与 PA2(USART2_TX)
     */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* 电机1方向脚 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 电机2方向脚 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    PWM_Init();
}

void Motor1_SetSpeed(int8_t Speed)
{
    if (Speed >= 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_4);
        GPIO_ResetBits(GPIOA, GPIO_Pin_5);
        PWM_SetCompare2(Speed);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_4);
        GPIO_SetBits(GPIOA, GPIO_Pin_5);
        PWM_SetCompare2(-Speed);
    }
}

void Motor2_SetSpeed(int8_t Speed)
{
    if (Speed >= 0)
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_10);
        GPIO_ResetBits(GPIOB, GPIO_Pin_11);
        PWM_SetCompare3(Speed);
    }
    else
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_10);
        GPIO_SetBits(GPIOB, GPIO_Pin_11);
        PWM_SetCompare3(-Speed);
    }
}
