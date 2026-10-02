#ifndef HALL_BRIDGE_H
#define HALL_BRIDGE_H

#include "main.h"

#define HALL_BRIDGE_FRAME_SIZE 16U

extern volatile uint32_t g_hall_bridge_rx_count;
extern volatile uint32_t g_hall_bridge_xor_error_count;
extern volatile uint32_t g_hall_bridge_uart_error_count;
extern volatile uint32_t g_hall_bridge_timestamp;
extern volatile uint8_t g_hall_bridge_valid;
extern volatile uint8_t g_hall_bridge_last_frame[HALL_BRIDGE_FRAME_SIZE];

void HallBridge_Init(UART_HandleTypeDef *uart);
void HallBridge_PushByte(uint8_t byte, uint32_t now_ms);
uint8_t HallBridge_CopyFreshFrame(uint8_t out[HALL_BRIDGE_FRAME_SIZE],
                                  uint32_t now_ms, uint32_t timeout_ms);
void HallBridge_RxCpltCallback(UART_HandleTypeDef *uart);
void HallBridge_ErrorCallback(UART_HandleTypeDef *uart);

#endif
