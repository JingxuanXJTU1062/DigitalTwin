#ifndef ESP8266_AT_EVENTS_H
#define ESP8266_AT_EVENTS_H

#include <stdint.h>

#define ESP8266_AT_EVENT_PROMPT   0x01U
#define ESP8266_AT_EVENT_SEND_OK  0x02U
#define ESP8266_AT_EVENT_BUSY     0x04U
#define ESP8266_AT_EVENT_ERROR    0x08U
#define ESP8266_AT_EVENT_FAIL     0x10U

void Esp8266AtEvents_Init(void);
void Esp8266AtEvents_Clear(void);
void Esp8266AtEvents_PushByte(uint8_t byte);
uint32_t Esp8266AtEvents_Get(void);

#endif
