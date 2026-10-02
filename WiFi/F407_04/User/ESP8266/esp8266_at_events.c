#include "esp8266_at_events.h"

#define AT_WINDOW_SIZE 8U

static uint8_t s_window[AT_WINDOW_SIZE];
static uint8_t s_window_length;
static volatile uint32_t s_events;

static uint8_t ends_with(const char *token, uint8_t token_length)
{
    uint8_t i;
    uint8_t start;
    if (s_window_length < token_length) return 0U;
    start = (uint8_t)(s_window_length - token_length);
    for (i = 0U; i < token_length; ++i) {
        if (s_window[start + i] != (uint8_t)token[i]) return 0U;
    }
    return 1U;
}

void Esp8266AtEvents_Init(void)
{
    s_window_length = 0U;
    s_events = 0U;
}

void Esp8266AtEvents_Clear(void)
{
    /* A naturally aligned 32-bit store is atomic on Cortex-M4.  This is
     * called before a new AT phase, before its response can arrive. */
    s_window_length = 0U;
    s_events = 0U;
}

void Esp8266AtEvents_PushByte(uint8_t byte)
{
    uint8_t i;
    if (byte == '>') s_events |= ESP8266_AT_EVENT_PROMPT;

    if (s_window_length < AT_WINDOW_SIZE) {
        s_window[s_window_length++] = byte;
    } else {
        for (i = 1U; i < AT_WINDOW_SIZE; ++i) s_window[i - 1U] = s_window[i];
        s_window[AT_WINDOW_SIZE - 1U] = byte;
    }

    if (ends_with("SEND OK", 7U)) s_events |= ESP8266_AT_EVENT_SEND_OK;
    if (ends_with("busy", 4U)) s_events |= ESP8266_AT_EVENT_BUSY;
    if (ends_with("ERROR", 5U)) s_events |= ESP8266_AT_EVENT_ERROR;
    if (ends_with("FAIL", 4U)) s_events |= ESP8266_AT_EVENT_FAIL;
}

uint32_t Esp8266AtEvents_Get(void)
{
    return s_events;
}
