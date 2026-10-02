/**
  ******************************************************************************
  * @file    bsp_uart5_hall.h
  * @brief   UART5 driver for F407_04 HAL version
  *
  *  Pin mapping:
  *    UART5  (PC12=TX, PD2=RX)   <- F103chuliao05 (出料, 12B XOR 帧 0x05)
  *                                      仅用 PD2 (RX), PC12 悬空
  *
  *  与 bsp_uart4_usart1.c 同构: HAL_UART_Init + __HAL_UART_ENABLE_IT(RXNE)
  *  UART5_IRQHandler 直接读取 SR/DR，并将字节送入协议环形队列.
  ******************************************************************************
  */
#ifndef __BSP_UART5_HALL_H
#define __BSP_UART5_HALL_H

#include "stm32f4xx_hal.h"

void UART5_HALL_Config(void);
void UART5_HALL_IRQHandler(void);
void UART5_HALL_Poll(void);

extern volatile uint32_t g_uart5_hall_irq_count;
extern volatile uint32_t g_uart5_hall_error_count;
extern volatile uint32_t g_uart5_hall_sr;
extern volatile uint32_t g_uart5_hall_cr1;
extern volatile uint32_t g_uart5_hall_brr;
extern volatile uint32_t g_uart5_hall_pd2_mode;
extern volatile uint32_t g_uart5_hall_pd2_af;
extern volatile uint8_t  g_uart5_hall_last_byte;
extern volatile HAL_StatusTypeDef g_uart5_hall_init_status;
extern volatile HAL_StatusTypeDef g_uart5_hall_rx_status;

#endif /* __BSP_UART5_HALL_H */
