/**
  ******************************************************************************
  * @file    esp8266.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   esp8266???????
  ******************************************************************************
  * @attention
  *
  * ?????:???  STM32 F407 ??????
  * ???    :http://www.firebbs.cn
  * ???    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
#include "./ESP8266/bsp_esp8266.h"
#include "./ESP8266/esp8266_at_events.h"
#include "./common/common.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"



static void                   ESP8266_GPIO_Config                 ( void );
static void                   ESP8266_USART_Config                ( void );
static void                   ESP8266_USART_NVIC_Configuration    ( void );


extern void Usart_SendString(uint8_t *str);

/* ESP8266 UART3 handle - defined in bsp_debug_usart.c, extern here */
extern UART_HandleTypeDef Uart3Handle;

/* Frame record structures - defined in bsp_esp8266.c */
struct STRUCT_USARTx_Fram strEsp8266_Fram_Record = { 0 };
struct STRUCT_USARTx_Fram strUSART_Fram_Record  = { 0 };
static void (*s_wait_hook)(void) = 0;
volatile uint32_t g_esp8266_send_ok_count = 0U;
volatile uint32_t g_esp8266_send_fail_count = 0U;
volatile uint32_t g_esp8266_send_retry_count = 0U;
volatile uint8_t g_esp8266_last_send_error = 0U;

static void ESP8266_ClearRxRecord(void)
{
    /* Runtime AT matching uses the ISR event state machine, so this legacy
     * diagnostic buffer can be reset without disabling RXNE.  A concurrent
     * append may be omitted from diagnostics but can no longer lose UART
     * hardware bytes or protocol events. */
    strEsp8266_Fram_Record.InfBit.FramLength = 0U;
    strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0U;
    strEsp8266_Fram_Record.Data_RX_BUF[0] = '\0';
}

void ESP8266_SetWaitHook(void (*hook)(void))
{
    s_wait_hook = hook;
}

/**
  * @brief  ESP8266?????????
  * @param  ??
  * @retval ??
  */
void ESP8266_Init ( void )
{
	Esp8266AtEvents_Init();
	ESP8266_GPIO_Config (); 
	
	ESP8266_USART_Config (); 
	
	
	macESP8266_RST_HIGH_LEVEL();

	macESP8266_CH_ENABLE();
	
	
}


/**
  * @brief  ?????ESP8266?????GPIO????
  * @param  ??
  * @retval ??
  */
static void ESP8266_GPIO_Config ( void )
{
	/*???????GPIO_InitTypeDef????????*/
	GPIO_InitTypeDef GPIO_InitStructure;


	/* ???? CH_PD ????*/
	 macESP8266_CH_PD_CLK_ENABLE();
											   
	GPIO_InitStructure.Pin = macESP8266_CH_PD_PIN;	

    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
   
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH; 

	HAL_GPIO_Init ( macESP8266_CH_PD_PORT, & GPIO_InitStructure );	 

	
	/* ???? RST ????*/
	macESP8266_RST_CLK_ENABLE();
											   
	GPIO_InitStructure.Pin = macESP8266_RST_PIN;	

	HAL_GPIO_Init ( macESP8266_RST_PORT, & GPIO_InitStructure );	 


}


/**
  * @brief  ?????ESP8266????? USART
  * @param  ??
  * @retval ??
  */
static void ESP8266_USART_Config ( void )
{
   
	/* USART3 mode config */
    Uart3Handle.Instance          = macESP8266_USARTx;
    
    Uart3Handle.Init.BaudRate     = macESP8266_USART_BAUD_RATE;
    Uart3Handle.Init.WordLength   = UART_WORDLENGTH_8B;
    Uart3Handle.Init.StopBits     = UART_STOPBITS_1;
    Uart3Handle.Init.Parity       = UART_PARITY_NONE;
    Uart3Handle.Init.Mode         = UART_MODE_TX_RX;
    Uart3Handle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    Uart3Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_Init(&Uart3Handle);
	
	/* ???????? */
	ESP8266_USART_NVIC_Configuration ();
    
    __HAL_UART_ENABLE_IT ( &Uart3Handle, USART_IT_RXNE); //????????????? 
	__HAL_UART_ENABLE_IT ( &Uart3Handle, USART_IT_IDLE); //????????????????? 	



}


/**
  * @brief  ???? ESP8266 USART ?? NVIC ????
  * @param  ??
  * @retval ??
  */
static void ESP8266_USART_NVIC_Configuration ( void )
{
//	HAL_NVIC_SetPriorityGrouping(macNVIC_PriorityGroup_x);
	/* Configure the NVIC Preemption Priority Bits */  
    HAL_NVIC_SetPriority(macESP8266_USART_IRQ,0,0);
    HAL_NVIC_EnableIRQ(macESP8266_USART_IRQ);

}


/*
 * ????????ESP8266_Rst
 * ????  ??????WF-ESP8266???
 * ????  ????
 * ????  : ??
 * ????  ???? ESP8266_AT_Test ????
 */
void ESP8266_Rst ( void )
{
	#if 0
	 ESP8266_Cmd ( "AT+RST", "OK", "ready", 2500 );   	
	
	#else
	 macESP8266_RST_LOW_LEVEL();
	 HAL_Delay ( 500 ); 
	 macESP8266_RST_HIGH_LEVEL();
	#endif

}


bool ESP8266_DHCP_CUR ( )
{
	char cCmd [40];

	sprintf ( cCmd, "AT+CWDHCP=1,1");
	
	return ESP8266_Cmd ( cCmd, "OK", NULL, 500 );
	
}

/*
 * ????????ESP8266_Cmd
 * ????  ????WF-ESP8266??????AT???
 * ????  ??cmd????????????
 *         reply1??reply2?????????????NULL????????????????????????
 *         waittime?????????????
 * ????  : 1?????????
 *         0??????????
 * ????  ??????????
 */
bool ESP8266_Cmd ( char * cmd, char * reply1, char * reply2, uint32_t waittime )
{    
    strEsp8266_Fram_Record .InfBit .FramLength = 0;               //??????????????????

	macESP8266_Usart ( "%s\r\n", cmd );

	if ( ( reply1 == 0 ) && ( reply2 == 0 ) )                      //?????????????
		return true;
	
    HAL_Delay ( waittime );                 //???
	
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ]  = '\0';

    macPC_Usart ( "?????????????%s", strEsp8266_Fram_Record .Data_RX_BUF );
  strEsp8266_Fram_Record .InfBit .FramLength = 0;                             //?????????
	strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;                             
	if ( ( reply1 != 0 ) && ( reply2 != 0 ) )
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply1 ) || 
						 ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply2 ) ); 
 	
	else if ( reply1 != 0 )
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply1 ) );
	
	else
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply2 ) );
	
}


/*
 * ????????ESP8266_AT_Test
 * ????  ????WF-ESP8266??????AT????????
 * ????  ????
 * ????  : ??
 * ????  ??????????
 */
//void ESP8266_AT_Test ( void )
//{
//	macESP8266_RST_HIGH_LEVEL();
//	
//	HAL_Delay ( 1000 ); 
//	
//	while ( ! ESP8266_Cmd ( "AT", "OK", NULL, 500 ) ) ESP8266_Rst ();  	

//}
bool ESP8266_AT_Test ( void )
{
	char count=0;
	
	macESP8266_RST_HIGH_LEVEL();	
  printf("\r\nAT????.....\r\n");
	HAL_Delay ( 2000 );
	while ( count < 10 )
	{
        printf("\r\nAT??????? %d......\r\n", count);
		if( ESP8266_Cmd ( "AT", "OK", NULL, 500 ) )
    {
      printf("\r\nAT??????????? %d......\r\n", count);
      return 1;
    }
		ESP8266_Rst();
		++ count;
	}
  return 0;
}


/*
 * ????????ESP8266_Net_Mode_Choose
 * ????  ?????WF-ESP8266?????????
 * ????  ??enumMode????????
 * ????  : 1???????
 *         0????????
 * ????  ??????????
 */
bool ESP8266_Net_Mode_Choose ( ENUM_Net_ModeTypeDef enumMode )
{
	switch ( enumMode )
	{
		case STA:
			return ESP8266_Cmd ( "AT+CWMODE=1", "OK", "no change", 2500 ); 
		
	  case AP:
		  return ESP8266_Cmd ( "AT+CWMODE=2", "OK", "no change", 2500 ); 
		
		case STA_AP:
		  return ESP8266_Cmd ( "AT+CWMODE=3", "OK", "no change", 2500 ); 
		
	  default:
		  return false;
  }
	
}


/*
 * ????????ESP8266_JoinAP
 * ????  ??WF-ESP8266?????????WiFi
 * ????  ??pSSID??WiFi?????????
 *       ??pPassWord??WiFi?????????
 * ????  : 1????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_JoinAP ( char * pSSID, char * pPassWord )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWJAP=\"%s\",\"%s\"", pSSID, pPassWord );
	
	return ESP8266_Cmd ( cCmd, "OK", NULL, 5000 );
	
}


/*
 * ????????ESP8266_BuildAP
 * ????  ??WF-ESP8266??????WiFi???
 * ????  ??pSSID??WiFi?????????
 *       ??pPassWord??WiFi?????????
 *       ??enunPsdMode??WiFi???????????????
 * ????  : 1?????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_BuildAP ( char * pSSID, char * pPassWord, ENUM_AP_PsdMode_TypeDef enunPsdMode )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWSAP=\"%s\",\"%s\",1,%d", pSSID, pPassWord, enunPsdMode );
	
	return ESP8266_Cmd ( cCmd, "OK", 0, 1000 );
	
}


/*
 * ????????ESP8266_Enable_MultipleId
 * ????  ??WF-ESP8266?????????????
 * ????  ??enumEnUnvarnishTx??????????????
 * ????  : 1?????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_Enable_MultipleId ( FunctionalState enumEnUnvarnishTx )
{
	char cStr [20];
	
	sprintf ( cStr, "AT+CIPMUX=%d", ( enumEnUnvarnishTx ? 1 : 0 ) );
	
	return ESP8266_Cmd ( cStr, "OK", 0, 500 );
	
}


/*
 * ????????ESP8266_Link_Server
 * ????  ??WF-ESP8266???????????????
 * ????  ??enumE??????????
 *       ??ip????????IP?????
 *       ??ComNum????????????????
 *       ??id????????????????ID
 * ????  : 1????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_Link_Server ( ENUM_NetPro_TypeDef enumE, char * ip, char * ComNum, ENUM_ID_NO_TypeDef id)
{
	char cStr [100] = { 0 }, cCmd [120];

  switch (  enumE )
  {
		case enumTCP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "TCP", ip, ComNum );
		  break;
		
		case enumUDP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "UDP", ip, ComNum );
		  break;
		
		default:
			break;
  }

  if ( id < 5 )
    sprintf ( cCmd, "AT+CIPSTART=%d,%s", id, cStr);

  else
	  sprintf ( cCmd, "AT+CIPSTART=%s", cStr );

	return ESP8266_Cmd ( cCmd, "OK", "ALREAY CONNECT", 4000 );
	
}


/*
 * ????????ESP8266_StartOrShutServer
 * ????  ??WF-ESP8266????????????????
 * ????  ??enumMode??????/???
 *       ??pPortNum?????????????????
 *       ??pTimeOver?????????????????????????????
 * ????  : 1?????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_StartOrShutServer ( FunctionalState enumMode, char * pPortNum, char * pTimeOver )
{
	char cCmd1 [120], cCmd2 [120];

	if ( enumMode )
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 1, pPortNum );
		
		sprintf ( cCmd2, "AT+CIPSTO=%s", pTimeOver );

		return ( ESP8266_Cmd ( cCmd1, "OK", 0, 500 ) &&
						 ESP8266_Cmd ( cCmd2, "OK", 0, 500 ) );
	}
	
	else
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 0, pPortNum );

		return ESP8266_Cmd ( cCmd1, "OK", 0, 500 );
	}
	
}


/*
 * ????????ESP8266_Get_LinkStatus
 * ????  ????? WF-ESP8266 ???????????????????????
 * ????  ????
 * ????  : 2?????ip
 *         3??????????
 *         3????????
 *         0??????????
 * ????  ??????????
 */
uint8_t ESP8266_Get_LinkStatus ( void )
{
	if ( ESP8266_Cmd ( "AT+CIPSTATUS", "OK", 0, 100 ) )
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:2\r\n" ) )
			return 2;
		
		else if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:3\r\n" ) )
			return 3;
		
		else if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:4\r\n" ) )
			return 4;		

	}
	
	return 0;
	
}


/*
 * ????????ESP8266_Get_IdLinkStatus
 * ????  ????? WF-ESP8266 ?????Id??????????????????????
 * ????  ????
 * ????  : ????Id??????????????5????????????????Id5~0?????????1????Id?????????????????0????Id??????????
 * ????  ??????????
 */
uint8_t ESP8266_Get_IdLinkStatus ( void )
{
	uint8_t ucIdLinkStatus = 0x00;
	
	
	if ( ESP8266_Cmd ( "AT+CIPSTATUS", "OK", 0, 100 ) )
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:0," ) )
			ucIdLinkStatus |= 0x01;
		else 
			ucIdLinkStatus &= ~ 0x01;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:1," ) )
			ucIdLinkStatus |= 0x02;
		else 
			ucIdLinkStatus &= ~ 0x02;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:2," ) )
			ucIdLinkStatus |= 0x04;
		else 
			ucIdLinkStatus &= ~ 0x04;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:3," ) )
			ucIdLinkStatus |= 0x08;
		else 
			ucIdLinkStatus &= ~ 0x08;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:4," ) )
			ucIdLinkStatus |= 0x10;
		else 
			ucIdLinkStatus &= ~ 0x10;	

	}
	
	return ucIdLinkStatus;
	
}


/*
 * ????????ESP8266_Inquire_ApIp
 * ????  ????? F-ESP8266 ?? AP IP
 * ????  ??pApIp????? AP IP ???????????
 *         ucArrayLength????? AP IP ??????????
 * ????  : 0????????
 *         1????????
 * ????  ??????????
 */
uint8_t ESP8266_Inquire_ApIp ( char * pApIp, uint8_t ucArrayLength )
{
	char uc;
	
	char * pCh;
	
	
  ESP8266_Cmd ( "AT+CIFSR", "OK", 0, 500 );
	
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "APIP,\"" );
	
	if ( pCh )
		pCh += 6;
	
	else
		return 0;
	
	for ( uc = 0; uc < ucArrayLength; uc ++ )
	{
		pApIp [ uc ] = * ( pCh + uc);
		
		if ( pApIp [ uc ] == '\"' )
		{
			pApIp [ uc ] = '\0';
			break;
		}
		
	}
	
	return 1;
	
}


/*
 * ????????ESP8266_UnvarnishSend
 * ????  ??????WF-ESP8266?????????????
 * ????  ????
 * ????  : 1?????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_UnvarnishSend ( void )
{
	if ( ! ESP8266_Cmd ( "AT+CIPMODE=1", "OK", 0, 500 ) )
		return false;
	
	return 
	  ESP8266_Cmd ( "AT+CIPSEND", "OK", ">", 500 );
	
}


/*
 * ????????ESP8266_ExitUnvarnishSend
 * ????  ??????WF-ESP8266???????????
 * ????  ????
 * ????  : ??
 * ????  ??????????
 */
void ESP8266_ExitUnvarnishSend ( void )
{
	HAL_Delay ( 1000 );
	
	macESP8266_Usart ( "+++" );
	
	HAL_Delay ( 500 ); 
	
}


/*
 * ????????ESP8266_SendString
 * ????  ??WF-ESP8266???????????
 * ????  ??enumEnUnvarnishTx?????????????????????
 *       ??pStr?????????????
 *       ??ulStrLength????????????????????
 *       ??ucId?????ID??????????
 * ????  : 1????????
 *         0?????????
 * ????  ??????????
 */
bool ESP8266_SendString ( FunctionalState enumEnUnvarnishTx, char * pStr, uint32_t ulStrLength, ENUM_ID_NO_TypeDef ucId )
{
	char cStr [20];
	bool bRet = false;


	if ( enumEnUnvarnishTx )
	{
		macESP8266_Usart ( "%s", pStr );

		bRet = true;

	}

	else
	{
		/* 关键: AT+CIPSEND 命令必须以 \r\n 结尾
		 * payload 长度就是实际 NMEA 字符串长度 (已含 \r\n) */
		if ( ucId < 5 )
			sprintf ( cStr, "AT+CIPSEND=%d,%d\r\n", ucId, ulStrLength );

		else
			sprintf ( cStr, "AT+CIPSEND=%d\r\n", ulStrLength );

		/* 清空 ESP8266 RX buffer, 准备接收 "> " 响应 */
		strEsp8266_Fram_Record.InfBit.FramLength = 0;
		strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;

		/* 发 AT+CIPSEND 命令 */
		macESP8266_Usart ( "%s", cStr );

		/* 短等待 "> " 响应 (ESP8266 处理 AT 命令 ~5-10ms)
		 * 用 SysTick 非阻塞延时, 避免阻塞主循环.
		 * 20ms 内 USART1/UART4 ISR push ring 数据 ~460 字节 (远小于 1024 ring). */
		{
			uint32_t t0 = HAL_GetTick();
			while ((HAL_GetTick() - t0) < 20) {
				if (strEsp8266_Fram_Record.InfBit.FramFinishFlag == 1) {
					if (strstr(strEsp8266_Fram_Record.Data_RX_BUF, ">")) {
						break; /* ESP8266 已进入等待数据模式 */
					}
				}
			}
		}

		/* 发 payload (含 \r\n), 不等 SEND OK (避免阻塞 500ms) */
		macESP8266_Usart ( "%s", pStr );
		bRet = true;
  }

	return bRet;

}

bool ESP8266_SendBytes(const uint8_t *data, uint16_t length, ENUM_ID_NO_TypeDef ucId)
{
    char command[32];
    uint32_t start;
    uint8_t prompt_seen = 0U;
    uint8_t attempt;
    uint8_t busy_seen;

    if (data == 0 || length == 0U || ucId >= 5) {
        g_esp8266_last_send_error = 1U;
        g_esp8266_send_fail_count++;
        return false;
    }
    sprintf(command, "AT+CIPSEND=%d,%d\r\n", ucId, length);

    /* Retry only before a payload is sent.  Retrying after payload would
     * risk duplicate Modbus responses on an otherwise valid TCP stream. */
    for (attempt = 0U; attempt < 2U && !prompt_seen; ++attempt) {
        uint32_t events;
        busy_seen = 0U;
        ESP8266_ClearRxRecord();
        Esp8266AtEvents_Clear();

        /* Send the AT command as one checked UART transfer.  The legacy
         * byte-at-a-time formatter ignored partial UART failures, which can
         * leave ESP8266 waiting for payload and leak the next AT command to
         * the TCP peer. */
        if (HAL_UART_Transmit(&Uart3Handle, (uint8_t *)command,
                              (uint16_t)strlen(command), 500U) != HAL_OK) {
            g_esp8266_last_send_error = 2U;
            g_esp8266_send_fail_count++;
            return false;
        }

        start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 500U) {
            if (s_wait_hook != 0) s_wait_hook();
            events = Esp8266AtEvents_Get();
            if ((events & ESP8266_AT_EVENT_PROMPT) != 0U) {
                prompt_seen = 1U;
                break;
            }
            if ((events & ESP8266_AT_EVENT_BUSY) != 0U) {
                busy_seen = 1U;
                break;
            }
            if ((events & (ESP8266_AT_EVENT_ERROR | ESP8266_AT_EVENT_FAIL)) != 0U) {
                g_esp8266_last_send_error = 3U;
                g_esp8266_send_fail_count++;
                return false;
            }
        }
        if (!prompt_seen && busy_seen && attempt == 0U) {
            uint32_t retry_start = HAL_GetTick();
            g_esp8266_send_retry_count++;
            while ((HAL_GetTick() - retry_start) < 50U) {
                if (s_wait_hook != 0) s_wait_hook();
            }
        } else if (!prompt_seen) {
            /* A lost prompt is ambiguous: ESP may already be waiting for
             * payload.  Never send another AT command in that state. */
            break;
        }
    }
    if (!prompt_seen) {
        g_esp8266_last_send_error = busy_seen ? 4U : 5U;
        g_esp8266_send_fail_count++;
        return false;
    }

    ESP8266_ClearRxRecord();
    Esp8266AtEvents_Clear();
    if (HAL_UART_Transmit(&Uart3Handle, (uint8_t *)data, length, 1000U) != HAL_OK) {
        g_esp8266_last_send_error = 6U;
        g_esp8266_send_fail_count++;
        return false;
    }

    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 1000U) {
        uint32_t events;
        if (s_wait_hook != 0) s_wait_hook();
        events = Esp8266AtEvents_Get();
        if ((events & ESP8266_AT_EVENT_SEND_OK) != 0U) {
            g_esp8266_last_send_error = 0U;
            g_esp8266_send_ok_count++;
            return true;
        }
        if ((events & (ESP8266_AT_EVENT_ERROR | ESP8266_AT_EVENT_FAIL)) != 0U) {
            g_esp8266_last_send_error = 7U;
            g_esp8266_send_fail_count++;
            return false;
        }
    }
    g_esp8266_last_send_error = 8U;
    g_esp8266_send_fail_count++;
    return false;
}


/*
 * ????????ESP8266_ReceiveString
 * ????  ??WF-ESP8266???????????
 * ????  ??enumEnUnvarnishTx?????????????????????
 * ????  : ????????????????
 * ????  ??????????
 */
char * ESP8266_ReceiveString ( FunctionalState enumEnUnvarnishTx )
{
	char * pRecStr = 0;
	
	
	strEsp8266_Fram_Record .InfBit .FramLength = 0;
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
	
	while ( ! strEsp8266_Fram_Record .InfBit .FramFinishFlag );
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ] = '\0';
	
	if ( enumEnUnvarnishTx )
		pRecStr = strEsp8266_Fram_Record .Data_RX_BUF;
	
	else 
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+IPD" ) )
			pRecStr = strEsp8266_Fram_Record .Data_RX_BUF;

	}

	return pRecStr;
	
}
