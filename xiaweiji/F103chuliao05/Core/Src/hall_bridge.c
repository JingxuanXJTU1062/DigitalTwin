#include "hall_bridge.h"

#define HALL_BRIDGE_HEADER_1    0xAAU
#define HALL_BRIDGE_HEADER_2    0x55U
#define HALL_BRIDGE_MODULE_ID   0x03U
#define HALL_BRIDGE_PAYLOAD_LEN 0x0BU

volatile uint32_t g_hall_bridge_rx_count = 0U;
volatile uint32_t g_hall_bridge_xor_error_count = 0U;
volatile uint32_t g_hall_bridge_uart_error_count = 0U;
volatile uint32_t g_hall_bridge_timestamp = 0U;
volatile uint8_t g_hall_bridge_valid = 0U;
volatile uint8_t g_hall_bridge_last_frame[HALL_BRIDGE_FRAME_SIZE];

static UART_HandleTypeDef *s_uart = NULL;
static uint8_t s_rx_byte = 0U;
static uint8_t s_rx_frame[HALL_BRIDGE_FRAME_SIZE];
static uint8_t s_rx_index = 0U;

static void HallBridge_ResetParser(uint8_t byte)
{
    s_rx_index = 0U;
    if (byte == HALL_BRIDGE_HEADER_1) {
        s_rx_frame[0] = byte;
        s_rx_index = 1U;
    }
}

void HallBridge_Init(UART_HandleTypeDef *uart)
{
    uint32_t i;
    s_uart = uart;
    s_rx_index = 0U;
    g_hall_bridge_rx_count = 0U;
    g_hall_bridge_xor_error_count = 0U;
    g_hall_bridge_uart_error_count = 0U;
    g_hall_bridge_timestamp = 0U;
    g_hall_bridge_valid = 0U;
    for (i = 0U; i < HALL_BRIDGE_FRAME_SIZE; ++i) {
        g_hall_bridge_last_frame[i] = 0U;
        s_rx_frame[i] = 0U;
    }
    if (s_uart != NULL) {
        (void)HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
    }
}

void HallBridge_PushByte(uint8_t byte, uint32_t now_ms)
{
    uint8_t checksum = 0U;
    uint32_t i;

    if (s_rx_index == 0U) {
        HallBridge_ResetParser(byte);
        return;
    }
    if ((s_rx_index == 1U) && (byte != HALL_BRIDGE_HEADER_2)) {
        HallBridge_ResetParser(byte);
        return;
    }
    if ((s_rx_index == 2U) && (byte != HALL_BRIDGE_MODULE_ID)) {
        HallBridge_ResetParser(byte);
        return;
    }
    if ((s_rx_index == 3U) && (byte != HALL_BRIDGE_PAYLOAD_LEN)) {
        HallBridge_ResetParser(byte);
        return;
    }

    s_rx_frame[s_rx_index++] = byte;
    if (s_rx_index < HALL_BRIDGE_FRAME_SIZE) {
        return;
    }

    for (i = 2U; i < 15U; ++i) {
        checksum ^= s_rx_frame[i];
    }
    if (checksum == s_rx_frame[15]) {
        for (i = 0U; i < HALL_BRIDGE_FRAME_SIZE; ++i) {
            g_hall_bridge_last_frame[i] = s_rx_frame[i];
        }
        g_hall_bridge_timestamp = now_ms;
        g_hall_bridge_valid = 1U;
        g_hall_bridge_rx_count++;
    } else {
        g_hall_bridge_xor_error_count++;
    }
    s_rx_index = 0U;
}

uint8_t HallBridge_CopyFreshFrame(uint8_t out[HALL_BRIDGE_FRAME_SIZE],
                                  uint32_t now_ms, uint32_t timeout_ms)
{
    uint32_t i;
    uint32_t primask;
    if (out == NULL) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if ((!g_hall_bridge_valid) ||
        ((uint32_t)(now_ms - g_hall_bridge_timestamp) > timeout_ms)) {
        if (primask == 0U) __enable_irq();
        return 0U;
    }
    for (i = 0U; i < HALL_BRIDGE_FRAME_SIZE; ++i) {
        out[i] = g_hall_bridge_last_frame[i];
    }
    if (primask == 0U) __enable_irq();
    return 1U;
}

void HallBridge_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if ((s_uart != NULL) && (uart == s_uart)) {
        HallBridge_PushByte(s_rx_byte, HAL_GetTick());
        (void)HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
    }
}

void HallBridge_ErrorCallback(UART_HandleTypeDef *uart)
{
    if ((s_uart != NULL) && (uart == s_uart)) {
        g_hall_bridge_uart_error_count++;
        (void)HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
    }
}
