/**
 ******************************************************************************
 * @file    distance_tx.c
 * @brief   Distance + Hall data transmission - 16-byte frame
 ******************************************************************************
 */
#include "distance_tx.h"

#define DISTANCE_TX_HEADER_1       0xAAU
#define DISTANCE_TX_HEADER_2      0x55U
#define DISTANCE_TX_MODULE_ID     0x03U  /* F407HALL: 3霍尔+2测距 */
#define DISTANCE_TX_PAYLOAD_SIZE  0x0BU  /* data length */

volatile HAL_StatusTypeDef g_uart_tx_status = HAL_OK;
volatile uint32_t g_uart_tx_count = 0U;
volatile uint32_t g_uart_tx_error_count = 0U;
volatile uint32_t g_uart_tx_last_tick_ms = 0U;
volatile uint8_t g_uart_last_frame[DISTANCE_TX_FRAME_SIZE];

static UART_HandleTypeDef *distance_tx_uart = NULL;

static uint8_t DistanceTx_CalcXor(const uint8_t *frame)
{
    uint8_t checksum = 0U;
    for (uint32_t i = 2U; i < (DISTANCE_TX_FRAME_SIZE - 1U); i++) {
        checksum ^= frame[i];
    }
    return checksum;
}

void DistanceTx_Init(UART_HandleTypeDef *uart)
{
    distance_tx_uart = uart;
    g_uart_tx_status = HAL_OK;
    g_uart_tx_count = 0U;
    g_uart_tx_error_count = 0U;
    g_uart_tx_last_tick_ms = 0U;
    for (uint32_t i = 0U; i < DISTANCE_TX_FRAME_SIZE; i++) {
        g_uart_last_frame[i] = 0U;
    }
}

HAL_StatusTypeDef DistanceTx_Send(uint32_t omega1_mrad,
                                  uint32_t omega2_mrad,
                                  uint32_t omega3_mrad,
                                  uint16_t dist1_mm,
                                  uint16_t dist2_mm,
                                  uint8_t range_status)
{
    uint8_t frame[DISTANCE_TX_FRAME_SIZE];

    /* Wire format is uint16_t mrad/s. Saturate instead of wrapping. */
    if (omega1_mrad > 65534U) omega1_mrad = 0U;
    if (omega2_mrad > 65534U) omega2_mrad = 0U;
    if (omega3_mrad > 65534U) omega3_mrad = 0U;

    if (distance_tx_uart == NULL) {
        g_uart_tx_status = HAL_ERROR;
        g_uart_tx_error_count++;
        return HAL_ERROR;
    }

    frame[0] = DISTANCE_TX_HEADER_1;
    frame[1] = DISTANCE_TX_HEADER_2;
    frame[2] = DISTANCE_TX_MODULE_ID;
    frame[3] = DISTANCE_TX_PAYLOAD_SIZE;
    /* omega1: 2 bytes LE */
    frame[4] = (uint8_t)(omega1_mrad & 0xFFU);
    frame[5] = (uint8_t)((omega1_mrad >> 8) & 0xFFU);
    /* omega2: 2 bytes LE */
    frame[6] = (uint8_t)(omega2_mrad & 0xFFU);
    frame[7] = (uint8_t)((omega2_mrad >> 8) & 0xFFU);
    /* omega3: 2 bytes LE */
    frame[8] = (uint8_t)(omega3_mrad & 0xFFU);
    frame[9] = (uint8_t)((omega3_mrad >> 8) & 0xFFU);
    /* dist1: 2 bytes LE */
    frame[10] = (uint8_t)(dist1_mm & 0xFFU);
    frame[11] = (uint8_t)((dist1_mm >> 8) & 0xFFU);
    /* dist2: 2 bytes LE */
    frame[12] = (uint8_t)(dist2_mm & 0xFFU);
    frame[13] = (uint8_t)((dist2_mm >> 8) & 0xFFU);
    /* range_status */
    frame[14] = range_status;
    /* xor */
    frame[15] = DistanceTx_CalcXor(frame);

    for (uint32_t i = 0U; i < DISTANCE_TX_FRAME_SIZE; i++) {
        g_uart_last_frame[i] = frame[i];
    }

    g_uart_tx_status = HAL_UART_Transmit(distance_tx_uart, frame,
                                         DISTANCE_TX_FRAME_SIZE, 20U);
    if (g_uart_tx_status == HAL_OK) {
        g_uart_tx_count++;
        g_uart_tx_last_tick_ms = HAL_GetTick();
    } else {
        g_uart_tx_error_count++;
    }
    return g_uart_tx_status;
}
