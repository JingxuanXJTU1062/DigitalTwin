#ifndef UART_TX_H
#define UART_TX_H

#include "main.h"
#include "hall03_frame.h"

extern volatile uint8_t g_uart_last_frame[HALL03_FRAME_SIZE];
extern volatile uint32_t g_uart_tx_count;
extern volatile uint32_t g_uart_tx_error_count;

void UART_Tx_Init(UART_HandleTypeDef *uart);
HAL_StatusTypeDef UART_Tx_Send(uint32_t omega1_mrad, uint32_t omega2_mrad,
                              uint16_t dist1_mm, uint16_t dist2_mm,
                              uint16_t dist3_mm, uint16_t range_status);

#endif
