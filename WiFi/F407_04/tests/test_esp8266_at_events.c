#include <assert.h>
#include <stdint.h>
#include "esp8266_at_events.h"

static void push_text(const char *text)
{
    while (*text != '\0') Esp8266AtEvents_PushByte((uint8_t)*text++);
}

int main(void)
{
    Esp8266AtEvents_Init();
    push_text("\r\nOK\r\n>");
    assert((Esp8266AtEvents_Get() & ESP8266_AT_EVENT_PROMPT) != 0U);

    Esp8266AtEvents_Clear();
    push_text("Recv 69 bytes\r\nSEND ");
    push_text("OK\r\n");
    assert(Esp8266AtEvents_Get() == ESP8266_AT_EVENT_SEND_OK);

    Esp8266AtEvents_Clear();
    push_text("busy p...\r\n");
    assert((Esp8266AtEvents_Get() & ESP8266_AT_EVENT_BUSY) != 0U);

    Esp8266AtEvents_Clear();
    push_text("ERROR\r\n");
    assert((Esp8266AtEvents_Get() & ESP8266_AT_EVENT_ERROR) != 0U);

    Esp8266AtEvents_Clear();
    push_text("SEND FAIL\r\n");
    assert((Esp8266AtEvents_Get() & ESP8266_AT_EVENT_FAIL) != 0U);
    return 0;
}
