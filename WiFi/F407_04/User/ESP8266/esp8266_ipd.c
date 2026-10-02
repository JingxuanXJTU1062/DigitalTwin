#include "esp8266_ipd.h"
#include <string.h>

typedef struct {
    uint8_t data[ESP8266_IPD_RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} IpdRing;

static IpdRing s_links[ESP8266_IPD_LINK_COUNT];
static uint8_t s_state;
static uint8_t s_prefix_pos;
/* Long enough for: link,length,remote IPv4/IPv6 text,remote port. */
static char s_header[64];
static uint8_t s_header_len;
static uint8_t s_payload_link;
static uint16_t s_payload_remaining;
static uint8_t s_poll_link;
static volatile uint32_t s_overflow_count;
static uint8_t s_drop_payload;
static uint8_t s_control_state;
static uint8_t s_control_link;
static uint8_t s_control_match;
static volatile uint32_t s_closed_event_count;
static const char s_prefix[] = "+IPD,";
static const char s_closed_suffix[] = "CLOSED\r\n";

static uint8_t ring_push(uint8_t link, uint8_t byte)
{
    uint16_t next;
    if (link >= ESP8266_IPD_LINK_COUNT) return 0U;
    next = (uint16_t)((s_links[link].head + 1U) % ESP8266_IPD_RING_SIZE);
    if (next == s_links[link].tail) {
        s_overflow_count++;
        s_links[link].head = s_links[link].tail = 0U;
        return 0U;
    }
    s_links[link].data[s_links[link].head] = byte;
    s_links[link].head = next;
    return 1U;
}

static uint16_t ring_available(uint8_t link)
{
    return (uint16_t)((s_links[link].head + ESP8266_IPD_RING_SIZE - s_links[link].tail) % ESP8266_IPD_RING_SIZE);
}

static uint8_t ring_peek(uint8_t link, uint16_t offset)
{
    return s_links[link].data[(s_links[link].tail + offset) % ESP8266_IPD_RING_SIZE];
}

static uint8_t ring_pop(uint8_t link)
{
    uint8_t value = s_links[link].data[s_links[link].tail];
    s_links[link].tail = (uint16_t)((s_links[link].tail + 1U) % ESP8266_IPD_RING_SIZE);
    return value;
}

static uint8_t parse_header(void)
{
    uint16_t link = 0U;
    uint16_t length = 0U;
    uint8_t i = 0U;
    uint8_t digits = 0U;
    while (i < s_header_len && s_header[i] >= '0' && s_header[i] <= '9') {
        link = (uint16_t)(link * 10U + (uint16_t)(s_header[i++] - '0'));
        digits++;
    }
    if (digits == 0U || i >= s_header_len || s_header[i++] != ',' || link >= ESP8266_IPD_LINK_COUNT) return 0U;
    digits = 0U;
    while (i < s_header_len && s_header[i] >= '0' && s_header[i] <= '9') {
        length = (uint16_t)(length * 10U + (uint16_t)(s_header[i++] - '0'));
        digits++;
    }
    if (digits == 0U || length == 0U) return 0U;

    /* CIPDINFO=0: <link>,<length>
     * CIPDINFO=1: <link>,<length>,<remote_ip>,<remote_port>
     * Accept both so an older AT firmware cannot block TCP server startup. */
    if (i < s_header_len) {
        uint8_t field_start;
        if (s_header[i++] != ',') return 0U;
        field_start = i;
        while (i < s_header_len && s_header[i] != ',') i++;
        if (i == field_start || i >= s_header_len) return 0U;
        i++;
        digits = 0U;
        while (i < s_header_len && s_header[i] >= '0' && s_header[i] <= '9') {
            i++;
            digits++;
        }
        if (digits == 0U || i != s_header_len) return 0U;
    }
    if (i != s_header_len) return 0U;
    s_payload_link = (uint8_t)link;
    s_payload_remaining = length;
    return 1U;
}

void Esp8266Ipd_Init(void)
{
    uint8_t i;
    for (i = 0U; i < ESP8266_IPD_LINK_COUNT; ++i) s_links[i].head = s_links[i].tail = 0U;
    s_state = 0U;
    s_prefix_pos = 0U;
    s_header_len = 0U;
    s_payload_remaining = 0U;
    s_poll_link = 0U;
    s_overflow_count = 0U;
    s_drop_payload = 0U;
    s_control_state = 0U;
    s_control_link = 0U;
    s_control_match = 0U;
    s_closed_event_count = 0U;
}

uint8_t Esp8266Ipd_PushByte(uint8_t byte)
{
    if (s_state == 2U) {
        if (!s_drop_payload && !ring_push(s_payload_link, byte)) s_drop_payload = 1U;
        if (--s_payload_remaining == 0U) {
            s_state = 0U;
            s_prefix_pos = 0U;
            s_drop_payload = 0U;
        }
        return 1U;
    }
    if (s_state == 1U) {
        if (byte == ':') {
            if (parse_header()) s_state = 2U;
            else s_state = 0U;
            s_header_len = 0U;
            return 1U;
        }
        if (s_header_len >= sizeof(s_header) || byte == '\r' || byte == '\n') {
            s_state = 0U;
            s_header_len = 0U;
            return 1U;
        }
        s_header[s_header_len++] = (char)byte;
        return 1U;
    }

    if (byte == (uint8_t)s_prefix[s_prefix_pos]) {
        s_prefix_pos++;
        if (s_prefix_pos == (sizeof(s_prefix) - 1U)) {
            s_state = 1U;
            s_header_len = 0U;
            s_prefix_pos = 0U;
            return 1U;
        }
    } else {
        s_prefix_pos = (byte == '+') ? 1U : 0U;
    }
    return 0U;
}

uint16_t Esp8266Ipd_PollAdu(uint8_t *link_id, uint8_t *adu, uint16_t capacity)
{
    uint8_t tries;
    for (tries = 0U; tries < ESP8266_IPD_LINK_COUNT; ++tries) {
        uint8_t link = s_poll_link;
        uint16_t available;
        uint16_t mbap_length;
        uint16_t total;
        uint16_t i;
        s_poll_link = (uint8_t)((s_poll_link + 1U) % ESP8266_IPD_LINK_COUNT);
        available = ring_available(link);
        while (available >= 7U) {
            mbap_length = (uint16_t)(((uint16_t)ring_peek(link, 4U) << 8) | ring_peek(link, 5U));
            total = (uint16_t)(6U + mbap_length);
            if (ring_peek(link, 2U) != 0U || ring_peek(link, 3U) != 0U || mbap_length < 2U || total > 260U) {
                (void)ring_pop(link);
                available--;
                continue;
            }
            if (available < total) break;
            if (capacity < total || adu == 0 || link_id == 0) return 0U;
            for (i = 0U; i < total; ++i) adu[i] = ring_pop(link);
            *link_id = link;
            return total;
        }
    }
    return 0U;
}

uint32_t Esp8266Ipd_GetOverflowCount(void)
{
    return s_overflow_count;
}

void Esp8266Ipd_ResetLink(uint8_t link_id)
{
    if (link_id >= ESP8266_IPD_LINK_COUNT) return;
    s_links[link_id].head = s_links[link_id].tail = 0U;
    if (s_state == 2U && s_payload_link == link_id) {
        /* The peer can close in the middle of +IPD.  Do not let a reused
         * ESP8266 link continue consuming bytes from that abandoned frame. */
        s_state = 0U;
        s_prefix_pos = 0U;
        s_header_len = 0U;
        s_payload_remaining = 0U;
        s_drop_payload = 0U;
    }
}

static void consume_closed_event(char *text, const char *pattern, uint8_t link_id)
{
    char *match = text;
    while ((match = strstr(match, pattern)) != 0) {
        Esp8266Ipd_ResetLink(link_id);
        /* Mark the event as consumed without inserting NUL, because later
         * AT replies in the same receive buffer must remain searchable. */
        match[2] = 'c';
        match += 3;
    }
}

void Esp8266Ipd_HandleControlText(char *text)
{
    if (text == 0) return;

    /* Consume CLOSED exactly once.  Do not reset on CONNECT:
     * ESP8266 can deliver "n,CONNECT" and the client's first +IPD request
     * in the same UART idle batch, after that request is already assembled.
     * Marking the C lowercase prevents historical CLOSED text from clearing
     * every later request when the buffer has not yet been reused. */
    consume_closed_event(text, "0,CLOSED", 0U);
    consume_closed_event(text, "1,CLOSED", 1U);
    consume_closed_event(text, "2,CLOSED", 2U);
    consume_closed_event(text, "3,CLOSED", 3U);
    consume_closed_event(text, "4,CLOSED", 4U);
}

void Esp8266Ipd_PushControlByte(uint8_t byte)
{
    if (s_control_state == 0U) {
        if (byte >= '0' && byte < ('0' + ESP8266_IPD_LINK_COUNT)) {
            s_control_link = (uint8_t)(byte - '0');
            s_control_state = 1U;
        }
        return;
    }

    if (s_control_state == 1U) {
        if (byte == ',') {
            s_control_state = 2U;
            s_control_match = 0U;
            return;
        }
        s_control_state = 0U;
        if (byte >= '0' && byte < ('0' + ESP8266_IPD_LINK_COUNT)) {
            s_control_link = (uint8_t)(byte - '0');
            s_control_state = 1U;
        }
        return;
    }

    if (byte == (uint8_t)s_closed_suffix[s_control_match]) {
        s_control_match++;
        if (s_control_match == (sizeof(s_closed_suffix) - 1U)) {
            Esp8266Ipd_ResetLink(s_control_link);
            s_closed_event_count++;
            s_control_state = 0U;
            s_control_match = 0U;
        }
        return;
    }

    s_control_state = 0U;
    s_control_match = 0U;
    if (byte >= '0' && byte < ('0' + ESP8266_IPD_LINK_COUNT)) {
        s_control_link = (uint8_t)(byte - '0');
        s_control_state = 1U;
    }
}

uint32_t Esp8266Ipd_GetClosedEventCount(void)
{
    return s_closed_event_count;
}
