/**
  ******************************************************************************
  * @file    bsp_debug_usart.h
  * @brief   USART1 + USART3 (ESP8266) HAL driver header
  *
  *  USART1 (PA9/PA10) <- F103 chuliao05
  *  USART3 (PB10/PB11) <- ESP8266
  ******************************************************************************
  */
#ifndef __DEBUG_USART_H
#define __DEBUG_USART_H

#include "stm32f4xx_hal.h"
#include <stdio.h>

#define DEBUG_USART_BAUDRATE   115200

#define DEBUG_USART            USART1
#define DEBUG_USART_TX_PIN    GPIO_PIN_9
#define DEBUG_USART_RX_PIN    GPIO_PIN_10
#define DEBUG_USART_TX_AF     GPIO_AF7_USART1
#define DEBUG_USART_RX_AF     GPIO_AF7_USART1

#define DEBUG_USART_IRQn       USART1_IRQn

void Usart_SendString(uint8_t *str);
void DEBUG_USART_Config(void);

extern UART_HandleTypeDef UartHandle;
extern UART_HandleTypeDef Uart3Handle;
extern volatile uint32_t g_esp8266_uart_rx_count;
extern volatile uint32_t g_esp8266_uart_idle_count;
extern volatile uint32_t g_esp8266_uart_ore_count;

#endif /* __DEBUG_USART_H */
