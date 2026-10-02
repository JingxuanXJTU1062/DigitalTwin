#include "stm32f10x.h"                  // Device header
#include "PWM.h"

void Motor_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_5| GPIO_Pin_6 | GPIO_Pin_7;  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    PWM_Init();     // 里边要初始化两个 PWM 通道：比如 TIM2_CH2 和 TIM2_CH3
}

/**
 * 电机1：使用某个 PWM 通道，比如 TIM2_CH2
 */
void Motor1_SetSpeed(int8_t Speed)
{
    if (Speed >= 0)
    {
        // 方向：正转
        GPIO_SetBits(GPIOA, GPIO_Pin_4);
        GPIO_ResetBits(GPIOA, GPIO_Pin_5);
        PWM_SetCompare2(Speed);    // 注意：PWM_SetCompare2 你要在 PWM.c 里实现
    }
    else
    {
        // 方向：反转
        GPIO_ResetBits(GPIOA, GPIO_Pin_4);
        GPIO_SetBits(GPIOA, GPIO_Pin_5);
        PWM_SetCompare2(-Speed);   // 取绝对值作为占空比
    }
}

/**
 * 电机2：使用另一个 PWM 通道，比如 TIM2_CH3
 */
void Motor2_SetSpeed(int8_t Speed)
{
    if (Speed >= 0)
    {
        
        GPIO_SetBits(GPIOA, GPIO_Pin_6);
        GPIO_ResetBits(GPIOA, GPIO_Pin_7);
        PWM_SetCompare3(Speed);    
    }
    else
    {
        // 方向：反转
        GPIO_ResetBits(GPIOA, GPIO_Pin_6);
        GPIO_SetBits(GPIOA, GPIO_Pin_7);
        PWM_SetCompare3(-Speed);
    }
}
