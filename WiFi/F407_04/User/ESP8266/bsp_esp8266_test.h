/**
  ******************************************************************************
  * @file    bsp_esp8266_test.h
  * @brief   ESP8266 AP mode + TCP server config (no MQTT)
  *
  *  SSID: BinghuoLink / PWD: wildfire123
  *  Modbus TCP Server: 192.168.4.1:502  (CIPMUX=1, multi-link)
  ******************************************************************************
  */
#ifndef __BSP_ESP8266_TEST_H
#define __BSP_ESP8266_TEST_H

#include "stm32f4xx.h"

/* ============ WiFi AP credentials ============ */
#define macUser_ESP8266_ApSsid       "BinghuoLink"
#define macUser_ESP8266_ApPwd        "wildfire123"
#define macUser_ESP8266_TcpServer_Port    "502"
#define macUser_ESP8266_TcpServer_Timeout  "1800"   /* 30 min timeout */

/* ============ External variables ============ */
extern volatile uint8_t ucTcpClosedFlag;

/* ============ Function declarations ============ */
void ESP8266_StaTcpClient_Unvarnish_ConfigTest(void);

#endif /* __BSP_ESP8266_TEST_H */
