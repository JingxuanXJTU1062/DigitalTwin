#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "stm32f10x.h"
#include "delay.h"   // 使用电机工程自己的 delay

// 兼容激光工程的接口名：Delay_us / Delay_ms
// 我们等下会在 delay.c 里新增 delay_us()
void delay_us(uint32_t us);

#define Delay_us(x)      delay_us((uint32_t)(x))
#define Delay_ms(x)      delay_ms((int32_t)(x))

// 某些文件里会调用 SysTick_Delay_Ms / SysTick_Delay_Us
static __inline void SysTick_Delay_Ms(__IO uint32_t ms)
{
    delay_ms((int32_t)ms);
}
static __inline void SysTick_Delay_Us(__IO uint32_t us)
{
    delay_us((uint32_t)us);
}

// 激光工程里还有 SysTick_Init() 声明，这里给个空壳声明（用不到也没关系）
static __inline void SysTick_Init(void) { }

#endif /* __SYSTICK_H */
