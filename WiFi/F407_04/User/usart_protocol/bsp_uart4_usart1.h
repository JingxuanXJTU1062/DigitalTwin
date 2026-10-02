/**
  ******************************************************************************
  * @file    bsp_uart4_usart1.h
  * @brief   UART4 driver for F407_04 HAL version
  *
  *  Pin mapping:
  *    UART4  (PC10=TX, PC11=RX)  <- F103 jinliao01 (进料, 10-byte frame)
  *    USART1 (PA9/PA10)           <- F103 chuliao05 (出料, 12-byte frame)
  *                                    -> handled by bsp_debug_usart.c ISR
  *    USART3 (PB10/PB11)          <- ESP8266 (已有, 不冲突)
  ******************************************************************************
  */
#ifndef __BSP_UART4_USART1_H
#define __BSP_UART4_USART1_H

#include "stm32f4xx_hal.h"

void UART4_USART1_Config(void);

#endif /* __BSP_UART4_USART1_H */
