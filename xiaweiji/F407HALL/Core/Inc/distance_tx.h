/**
 ******************************************************************************
 * @file    distance_tx.h
 * @brief   Distance + Hall data transmission - 16-byte frame
 *
 * Frame format (16 bytes, ID=0x03):
 *   [0] 0xAA, [1] 0x55, [2] 0x03, [3] 0x0B,
 *   [4-5] omega1, [6-7] omega2, [8-9] omega3 (LE mrad/s),
 *   [10-11] dist1, [12-13] dist2 (LE mm),
 *   [14] range_status, [15] xor
 ******************************************************************************
 */
#ifndef DISTANCE_TX_H
#define DISTANCE_TX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define DISTANCE_TX_FRAME_SIZE  16U

extern volatile HAL_StatusTypeDef g_uart_tx_status;
extern volatile uint32_t g_uart_tx_count;
extern volatile uint32_t g_uart_tx_error_count;
extern volatile uint32_t g_uart_tx_last_tick_ms;
extern volatile uint8_t g_uart_last_frame[DISTANCE_TX_FRAME_SIZE];

void DistanceTx_Init(UART_HandleTypeDef *uart);
HAL_StatusTypeDef DistanceTx_Send(uint32_t omega1_mrad,
                                  uint32_t omega2_mrad,
                                  uint32_t omega3_mrad,
                                  uint16_t dist1_mm,
                                  uint16_t dist2_mm,
                                  uint8_t range_status);

#ifdef __cplusplus
}
#endif

#endif /* DISTANCE_TX_H */
