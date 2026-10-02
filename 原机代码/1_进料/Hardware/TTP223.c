#include "ttp223.h"

static volatile uint8_t TTP223_TouchFlag = 0;

/**
  * @brief  TTP223初始化：PA6输入 + EXTI上升沿中断
  * @param  None
  * @retval None
  */
void TTP223_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 开启时钟
    RCC_APB2PeriphClockCmd(TTP223_GPIO_CLK | TTP223_AFIO_CLK, ENABLE);

    // 2. PA6 配置为下拉输入
    GPIO_InitStructure.GPIO_Pin = TTP223_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TTP223_GPIO_PORT, &GPIO_InitStructure);

    // 3. EXTI 映射
    GPIO_EXTILineConfig(TTP223_PORT_SOURCE, TTP223_PIN_SOURCE);

    // 4. EXTI 配置：上升沿触发
    EXTI_InitStructure.EXTI_Line = TTP223_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // 5. NVIC 配置
    NVIC_InitStructure.NVIC_IRQChannel = TTP223_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  读取TTP223当前电平状态
  * @param  None
  * @retval 1: 被触摸(高电平), 0: 未触摸(低电平)
  */
uint8_t TTP223_ReadState(void)
{
    return GPIO_ReadInputDataBit(TTP223_GPIO_PORT, TTP223_GPIO_PIN);
}

/**
  * @brief  获取触摸事件标志
  * @param  None
  * @retval 1: 检测到触摸上升沿事件, 0: 无事件
  */
uint8_t TTP223_GetTouchFlag(void)
{
    return TTP223_TouchFlag;
}

/**
  * @brief  清除触摸事件标志
  * @param  None
  * @retval None
  */
void TTP223_ClearTouchFlag(void)
{
    TTP223_TouchFlag = 0;
}

/**
  * @brief  EXTI9_5中断服务函数
  * @param  None
  * @retval None
  */
void EXTI9_5_IRQHandler(void)
{
    if(EXTI_GetITStatus(TTP223_EXTI_LINE) != RESET)
    {
        TTP223_TouchFlag = 1;

        EXTI_ClearITPendingBit(TTP223_EXTI_LINE);
    }
}
