/**
  ******************************************************************************
  * @file    bsp_esp8266_test.c
  * @brief   ESP8266 AP + TCP Server configuration (no MQTT)
  *
  *  SSID: BinghuoLink / PWD: wildfire123
  *  Modbus TCP Server on 192.168.4.1:502 (CIPMUX=1, multi-link)
  *
  *  Data is sent per-link via AT+CIPSEND=<link_id>,<len>
  *  (API: ESP8266_SendString with Multiple_ID_0..4)
  ******************************************************************************
  */
#include "./ESP8266/bsp_esp8266_test.h"
#include "./ESP8266/bsp_esp8266.h"
#include <stdbool.h>

volatile uint8_t ucTcpClosedFlag = 0;

/**
  * @brief  Configure ESP8266 as AP + TCP server (no transparent mode).
  *
  *  After this returns, the ESP8266 will:
  *    - broadcast an SSID "BinghuoLink" / pwd "wildfire123"
  *    - run a TCP server on 192.168.4.1:502
  *    - data is sent per-link via AT+CIPSEND=<link_id>,<len>
  *      (API: ESP8266_SendString with Multiple_ID_0..4)
  */
void ESP8266_StaTcpClient_Unvarnish_ConfigTest(void)
{
    /* 1) AT test */
    while (!ESP8266_AT_Test());

    /* 1.5) 关掉 AT 命令回显, 防止 ESP8266 把 CIPSEND 命令转发给 TCP 客户端 */
    while (!ESP8266_Cmd("ATE0", "OK", NULL, 500));

    /* 2) set mode = AP */
    while (!ESP8266_Net_Mode_Choose(AP));

    /* 3) build AP */
    while (!ESP8266_BuildAP(
        macUser_ESP8266_ApSsid,
        macUser_ESP8266_ApPwd,
        WPA_WPA2_PSK));

    /* 4) enable multi-connection (CIPMUX=1) */
    while (!ESP8266_Enable_MultipleId(ENABLE));

    /* 可选配置：部分旧 AT 固件不支持 CIPDINFO，会返回 ERROR。
     * 不得在此无限重试，否则热点虽已建立，TCP server 永远不会启动。
     * +IPD 解析器同时兼容 CIPDINFO=0/1，因此失败可安全继续。 */
    (void)ESP8266_Cmd("AT+CIPDINFO=0", "OK", NULL, 500);

    /* 5) start TCP server */
    while (!ESP8266_StartOrShutServer(
        ENABLE,
        macUser_ESP8266_TcpServer_Port,
        macUser_ESP8266_TcpServer_Timeout));

    /* NOTE: CIPMODE=1 is incompatible with AP+TCP-server+CIPMUX=1.
     * Data must be sent via: ESP8266_SendString(DISABLE, buf, len, link_id) */
}
