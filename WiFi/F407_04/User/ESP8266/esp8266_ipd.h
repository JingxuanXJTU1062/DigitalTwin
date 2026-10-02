#ifndef ESP8266_IPD_H
#define ESP8266_IPD_H

#include <stdint.h>

#define ESP8266_IPD_LINK_COUNT       5U
#define ESP8266_IPD_RING_SIZE      512U

void Esp8266Ipd_Init(void);
uint8_t Esp8266Ipd_PushByte(uint8_t byte);
uint16_t Esp8266Ipd_PollAdu(uint8_t *link_id, uint8_t *adu, uint16_t capacity);
uint32_t Esp8266Ipd_GetOverflowCount(void);
void Esp8266Ipd_ResetLink(uint8_t link_id);
void Esp8266Ipd_HandleControlText(char *text);
void Esp8266Ipd_PushControlByte(uint8_t byte);
uint32_t Esp8266Ipd_GetClosedEventCount(void);

#endif
