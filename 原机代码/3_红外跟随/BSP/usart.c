#include "usart.h"
#include <stdio.h>

__IO bool rxFrameFlag = false;
__IO uint8_t rxCmd[FIFO_SIZE] = {0};
__IO uint8_t rxCount = 0;

#define MOTOR_CACHE_MAX_ADDR   256u

static volatile uint8_t s_motorAngleValid[MOTOR_CACHE_MAX_ADDR] = {0};
static volatile float   s_motorAngleDeg[MOTOR_CACHE_MAX_ADDR]   = {0.0f};

static void usart_try_parse_motor_auto_report(uint8_t *buf, uint8_t len)
{
    uint8_t addr;
    uint32_t pos;
    float angle;

    if (len != 8u) return;

    addr = buf[0];

    /* 实时位置返回帧：addr + 0x36 + dir + pos(4B) + 0x6B */
    if (buf[1] != 0x36) return;
    if (buf[7] != 0x6B) return;

    pos = ((uint32_t)buf[3] << 24) |
          ((uint32_t)buf[4] << 16) |
          ((uint32_t)buf[5] << 8)  |
          ((uint32_t)buf[6] << 0);

    angle = (float)pos * 360.0f / 65536.0f;
    if (buf[2]) angle = -angle;

    s_motorAngleDeg[addr] = angle;
    s_motorAngleValid[addr] = 1u;
}

/**
	* @brief   USART1中断函数
	* @param   无
	* @retval  无
	*/
void USART1_IRQHandler(void)
{
	__IO uint16_t i = 0;

/**********************************************************
***	串口接收中断
**********************************************************/
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		// 未完成一帧数据接收，数据进入缓冲队列
		fifo_enQueue((uint8_t)USART1->DR);

		// 清除串口接收中断
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}

/**********************************************************
***	串口空闲中断
**********************************************************/
	else if(USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
	{
		// 先读SR再读DR，清除IDLE中断
		USART1->SR; USART1->DR;

		// 提取一帧数据命令
		rxCount = fifo_queueLength(); for(i=0; i < rxCount; i++) { rxCmd[i] = fifo_deQueue(); }
		uint8_t is_auto = 0;

		if (rxCount == 8 && rxCmd[1] == 0x36 && rxCmd[7] == 0x6B)
		{
			usart_try_parse_motor_auto_report((uint8_t *)rxCmd, rxCount);
			is_auto = 1;
		}

		/* 只有非自动帧才触发rxFrameFlag */
		if (!is_auto)
		{
			rxFrameFlag = true;
		}
	}
}

/**
	* @brief   USART发送多个字节
	* @param   无
	* @retval  无
	*/
void usart_SendCmd(__IO uint8_t *cmd, uint8_t len)
{
	__IO uint8_t i = 0;
	
	for(i=0; i < len; i++) { usart_SendByte(cmd[i]); }
}

/**
	* @brief   USART发送一个字节
	* @param   无
	* @retval  无
	*/
void usart_SendByte(uint16_t data)
{
	__IO uint16_t t0 = 0;
	
	USART1->DR = (data & (uint16_t)0x01FF);

	while(!(USART1->SR & USART_FLAG_TXE))
	{
		++t0; if(t0 > 8000)	{	return; }
	}
}


int fputc(int ch, FILE *f)
{
    (void)f;
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, (uint8_t)ch);
    return ch;
}

bool usart_GetMotorAngleDeg(uint8_t addr, float *deg_out)
{
    if (deg_out == 0) return false;
    if (addr >= MOTOR_CACHE_MAX_ADDR) return false;
    if (!s_motorAngleValid[addr]) return false;

    *deg_out = s_motorAngleDeg[addr];
    return true;
}

void usart_ClearMotorAngleCache(uint8_t addr)
{
    if (addr >= MOTOR_CACHE_MAX_ADDR) return;

    s_motorAngleValid[addr] = 0u;
    s_motorAngleDeg[addr] = 0.0f;
}



