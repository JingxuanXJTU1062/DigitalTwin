#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp8266_ipd.h"

void Esp8266Ipd_PushControlByte(uint8_t byte);
uint32_t Esp8266Ipd_GetClosedEventCount(void);

int main(void)
{
    static const uint8_t stream[] = "+IPD,2,12:\x00\x01\x00\x00\x00\x06\x01\x04\x00\x00\x00\x1e";
    static const uint8_t stream_with_remote[] =
        "+IPD,3,12,192.168.4.2,54321:"
        "\x00\x04\x00\x00\x00\x06\x01\x04\x00\x00\x00\x02";
    static const uint8_t partial[] = "+IPD,1,12:\x00\x02\x00";
    static const uint8_t good_after_reset[] = "+IPD,1,12:\x00\x03\x00\x00\x00\x06\x01\x04\x00\x00\x00\x01";
    char connect_event[] = "2,CONNECT\r\n+IPD,2,12:";
    char close_event[] = "1,CLOSED\r\n";
    uint8_t link = 0;
    uint8_t adu[32];
    uint16_t i;
    Esp8266Ipd_Init();
    for (i = 0; i < sizeof(stream) - 1; ++i) Esp8266Ipd_PushByte(stream[i]);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12);
    assert(link == 2 && adu[7] == 4);

    /* CIPDINFO=1 adds remote IP and port; it must carry the same ADU. */
    Esp8266Ipd_Init();
    for (i = 0; i < sizeof(stream_with_remote) - 1; ++i) {
        Esp8266Ipd_PushByte(stream_with_remote[i]);
    }
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12U);
    assert(link == 3 && adu[1] == 4 && adu[7] == 4);

    /* ESP can emit "CONNECT" and the first +IPD request in one UART idle
     * batch.  CONNECT must not erase the already assembled first request. */
    Esp8266Ipd_Init();
    for (i = 0; i < sizeof(stream) - 1; ++i) Esp8266Ipd_PushByte(stream[i]);
    Esp8266Ipd_HandleControlText(connect_event);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12U);
    assert(link == 2);

    /* Closing/reusing a link must discard an incomplete payload. */
    Esp8266Ipd_Init();
    for (i = 0; i < sizeof(partial) - 1; ++i) Esp8266Ipd_PushByte(partial[i]);
    Esp8266Ipd_HandleControlText(close_event);
    assert(strstr(close_event, "1,CLOSED") == 0);
    for (i = 0; i < sizeof(good_after_reset) - 1; ++i) Esp8266Ipd_PushByte(good_after_reset[i]);
    /* Reprocessing the same UART history must not clear the new request. */
    Esp8266Ipd_HandleControlText(close_event);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12);
    assert(link == 1 && adu[1] == 3 && adu[7] == 4);

    /* ISR-safe streaming control parser consumes one physical CLOSED event
     * once, without rescanning historical AT response text. */
    Esp8266Ipd_Init();
    for (i = 0; i < sizeof(partial) - 1; ++i) Esp8266Ipd_PushByte(partial[i]);
    for (i = 0; close_event[i] != '\0'; ++i) {
        Esp8266Ipd_PushControlByte((uint8_t)close_event[i]);
    }
    assert(Esp8266Ipd_GetClosedEventCount() == 1U);
    for (i = 0; i < sizeof(good_after_reset) - 1; ++i) Esp8266Ipd_PushByte(good_after_reset[i]);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12U);

    /* Overflow invalidates the whole +IPD payload; a later clean frame works. */
    Esp8266Ipd_Init();
    {
        static const uint8_t header[] = "+IPD,0,520:";
        for (i = 0; i < sizeof(header) - 1; ++i) Esp8266Ipd_PushByte(header[i]);
        for (i = 0; i < 520U; ++i) Esp8266Ipd_PushByte((uint8_t)i);
    }
    assert(Esp8266Ipd_GetOverflowCount() == 1U);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 0U);
    for (i = 0; i < sizeof(stream) - 1; ++i) Esp8266Ipd_PushByte(stream[i]);
    assert(Esp8266Ipd_PollAdu(&link, adu, sizeof(adu)) == 12U);
    assert(link == 2);
    return 0;
}
