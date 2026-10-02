#include "usart.h"
#include <stdio.h>



__IO bool rxFrameFlag = false;
__IO uint8_t rxCmd[FIFO_SIZE] = {0};
__IO uint8_t rxCount = 0;

/**
	* @brief   USART1
	* @param   
	* @retval  
	*/
void USART1_IRQHandler(void)
{
	__IO uint16_t i = 0;


	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		
		fifo_enQueue((uint8_t)USART1->DR);

		
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}


	else if(USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
	{
		
		USART1->SR; USART1->DR;

	
		rxCount = fifo_queueLength(); for(i=0; i < rxCount; i++) { rxCmd[i] = fifo_deQueue(); }

		
		rxFrameFlag = true;
	}
}

/**
	* @brief   
	* @param   
	* @retval  
	*/
void usart_SendCmd(__IO uint8_t *cmd, uint8_t len)
{
	__IO uint8_t i = 0;
	
	for(i=0; i < len; i++) { usart_SendByte(cmd[i]); }
}

/**
	* @brief   
	* @param   
	* @retval  
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




#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    USART_SendData(USART2, (uint8_t)ch);
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    return ch;
}
