#include "uart_tx.h"

volatile uint8_t g_uart_last_frame[HALL03_FRAME_SIZE];
volatile uint32_t g_uart_tx_count = 0U;
volatile uint32_t g_uart_tx_error_count = 0U;
static UART_HandleTypeDef *s_uart = NULL;

void UART_Tx_Init(UART_HandleTypeDef *uart)
{
  s_uart = uart;
  for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) g_uart_last_frame[i] = 0U;
}

HAL_StatusTypeDef UART_Tx_Send(uint32_t omega1_mrad, uint32_t omega2_mrad,
                              uint16_t dist1_mm, uint16_t dist2_mm,
                              uint16_t dist3_mm, uint16_t range_status)
{
  uint8_t frame[HALL03_FRAME_SIZE];
  HAL_StatusTypeDef status;
  if (s_uart == NULL) return HAL_ERROR;
  Hall03Frame_Build(frame, omega1_mrad, omega2_mrad,
                    dist1_mm, dist2_mm, dist3_mm, range_status);
  for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) g_uart_last_frame[i] = frame[i];
  status = HAL_UART_Transmit(s_uart, frame, HALL03_FRAME_SIZE, 20U);
  if (status == HAL_OK) g_uart_tx_count++;
  else g_uart_tx_error_count++;
  return status;
}
