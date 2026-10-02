#include "delay.h"

/**
	*	@brief		毫秒级延时
	*	@param		int32_t u32Cnt
	*	@retval		无
	*/
void delay_ms(int32_t i32Cnt)
{
	__IO int32_t i32end = 0;

	SysTick->LOAD = 0xFFFFFF;
	SysTick->VAL  = 0;
	SysTick->CTRL = (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk);

	while(i32Cnt > 0)
	{
		SysTick->VAL = 0;
		i32end = 0x1000000 - (SystemCoreClock / 1000);
		while(SysTick->VAL > i32end);
		--i32Cnt;
	}

	SysTick->CTRL = (SysTick->CTRL & (~SysTick_CTRL_ENABLE_Msk));
}

/**
	*	@brief		软件延时
	*	@param		int32_t u32Cnt
	*	@retval		无
	*/
void delay_cnt(int32_t i32Cnt)
{
	while(i32Cnt > 0) { i32Cnt--; }
}
/**
  * @brief  微秒级延时（用于软件I2C等）
  * @param  us: 延时时长（微秒）
  */
void delay_us(uint32_t us)
{
    __IO int32_t end = 0;

    SysTick->LOAD = 0xFFFFFF;
    SysTick->VAL  = 0;
    SysTick->CTRL = (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk);

    while(us > 0)
    {
        SysTick->VAL = 0;

        // 1us 需要的时钟周期数 = SystemCoreClock / 1,000,000
        // 用和 delay_ms 同样的“阈值等待”写法
        end = 0x1000000 - (SystemCoreClock / 1000000);

        while(SysTick->VAL > end);
        us--;
    }

    SysTick->CTRL = (SysTick->CTRL & (~SysTick_CTRL_ENABLE_Msk));
}
