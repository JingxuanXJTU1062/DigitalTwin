#ifndef __USART_H
#define __USART_H

#include "board.h"
#include "fifo.h"



extern __IO bool rxFrameFlag;
extern __IO uint8_t rxCmd[FIFO_SIZE];
extern __IO uint8_t rxCount;

void usart_SendCmd(__IO uint8_t *cmd, uint8_t len);
void usart_SendByte(uint16_t data);
bool usart_GetMotorAngleDeg(uint8_t addr, float *deg_out);
void usart_ClearMotorAngleCache(uint8_t addr);

#endif
