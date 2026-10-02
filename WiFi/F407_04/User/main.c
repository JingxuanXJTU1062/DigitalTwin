/**
  ******************************************************************************
  * @file    main.c
  * @brief   Digital Twin Gateway - F407_04 + ESP8266
  *
  *  Pipeline:
  *    F103 jinliao01 --[UART4  PC10/PC11]--> F407 --[parse AA55]--> $JINLIAO NMEA
  *    F103 chuliao05 --[USART1 PA9/PA10 ]--> F407 --[parse AA55]--> $CHULIAO NMEA
  *                                                          \-->[ESP8266 USART3 PB10/PB11]
  *                                                               --> PC Modbus TCP client on 502
  *
  *  ESP8266 mode: AP "BinghuoLink" / pwd "wildfire123"
  *                TCP server on 192.168.4.1:502 (CIPMUX=1, multi-link)
  *
  *  Host client: mcu_wifi_listener.py (Modbus TCP, Unit ID 1)
  *
  *  Buffer sizing:
  *    tx_buf[512] must cover $FRAME combined-packet max length
  *    (max ~400 bytes: J+C+P+D+I+*CS+\r\n).
  *    A 256-byte buffer causes snprintf truncation that drops *CS\r\n,
  *    making PC-side checksum check fail → "格式未知".
  ******************************************************************************
  */
#include "main.h"
#include "stm32f4xx_hal.h"
#include "./usart/bsp_debug_usart.h"
#include "./ESP8266/bsp_esp8266.h"
#include "./ESP8266/bsp_esp8266_test.h"
#include "./ESP8266/wifi_cmd.h"
#include "./ESP8266/esp8266_ipd.h"
#include "./Modbus/modbus_tcp_server.h"
#include "./usart_protocol/usart_protocol.h"
#include "./usart_protocol/bsp_uart4_usart1.h"
#include "./usart_protocol/bsp_uart5_hall.h"
#include <string.h>

#define SNAPSHOT_INTERVAL_MS    100
#define SENSOR_TIMEOUT_MS       500
#define COMMAND_TIMEOUT_MS      1500
static void ModbusCoilWrite(uint8_t index, uint8_t value)
{
    wifi_cmd_set_output(index, value);
}

static void CommunicationWaitHook(void)
{
    uint32_t now = HAL_GetTick();
    Protocol_ExpireStaleData(now, SENSOR_TIMEOUT_MS);
    ModbusTcp_ApplySafetyTimeout(now, COMMAND_TIMEOUT_MS);
}

int main(void){
    g_esp8266_send_ok_count;
    uint32_t last_snapshot_tick = 0;
    uint16_t sequence = 0U;
    uint16_t input_registers[MODBUS_INPUT_REGISTER_COUNT];
    uint8_t request_adu[MODBUS_TCP_MAX_ADU_SIZE];
    uint8_t response_adu[MODBUS_TCP_MAX_ADU_SIZE];

    /* Hardware init */
    HAL_Init();
    SystemClock_Config();

    /* USART1 (chuliao05) + USART3 (ESP8266) */
    DEBUG_USART_Config();

    /* UART4 (jinliao01) */
    UART4_USART1_Config();

    /* UART5 (F103chuliao05 出料 0x05) */
    UART5_HALL_Config();

    /* ESP8266 (USART3 + CH_PD/RST GPIO) */
    ESP8266_Init();

    Esp8266Ipd_Init();
    ModbusTcp_Init(ModbusCoilWrite);
    ESP8266_SetWaitHook(CommunicationWaitHook);

    /* ESP8266 AP mode + TCP server */
    ESP8266_StaTcpClient_Unvarnish_ConfigTest();

    /* 初始化 PC 下发命令的 3 个 GPIO (PE4/PE5/PE6), 初始低电平 */
    wifi_cmd_init();

    last_snapshot_tick = HAL_GetTick();

    while (1)
    {
        uint32_t now = HAL_GetTick();

        Protocol_ExpireStaleData(now, SENSOR_TIMEOUT_MS);
        ModbusTcp_ApplySafetyTimeout(now, COMMAND_TIMEOUT_MS);

        /* UART5 轮询兜底：正常由 RXNE IRQ 接收，NVIC 异常时仍可接收。 */
        UART5_HALL_Poll();

        /* 1. USART1 (F103jinliao02 进料2: 霍尔+压力) */
        Protocol_USART1_RxCpltCallback();
        Protocol_ProcessBuffer(UART_IDX_USART1);

        /* 2. UART4 (F103jinliao01 进料1) */
        Protocol_UART4_RxCpltCallback();
        Protocol_ProcessBuffer(UART_IDX_UART4);

        /* 2.2 USART2 (F407HALL03 2路霍尔+3路测距) */
        Protocol_ProcessBuffer(UART_IDX_USART2);

        /* 2.3 UART5 (F103chuliao05) */
        Protocol_ProcessBuffer(UART_IDX_UART5_HALL);

        /* 3. 每100ms更新 Modbus 输入寄存器快照。 */
        if ((now - last_snapshot_tick) >= SNAPSHOT_INTERVAL_MS) {
            last_snapshot_tick = now;

            Protocol_BuildModbusInputRegisters(input_registers,
                MODBUS_INPUT_REGISTER_COUNT, sequence++,
                ModbusTcp_CommandIsFresh(now, COMMAND_TIMEOUT_MS));
            ModbusTcp_SetInputRegisters(input_registers, MODBUS_INPUT_REGISTER_COUNT);
        }

        /* 处理所有已重组的 Modbus TCP ADU，并回到原 TCP link。 */
        {
            uint8_t link_id;
            uint16_t request_len = Esp8266Ipd_PollAdu(&link_id, request_adu,
                                                       sizeof(request_adu));
            uint16_t response_len;
            if (request_len > 0U) {
                response_len = ModbusTcp_ProcessAdu(request_adu, request_len,
                                                     response_adu, sizeof(response_adu), now);
                if (response_len > 0U) {
                    if (!ESP8266_SendBytes(response_adu, response_len,
                                           (ENUM_ID_NO_TypeDef)link_id)) {
                        /* Drop any queued bytes from this transaction.  The
                         * diagnostic counters in bsp_esp8266.c retain the
                         * exact failure category for Keil Watch. */
                        Esp8266Ipd_ResetLink(link_id);
                    }
                }
            }
        }

    }
}

/**
  * @brief  System Clock Configuration
  *         System Clock source = PLL (HSE 25MHz)
  *         SYSCLK(Hz)        = 168000000
  *         HCLK(Hz)          = 168000000
  *         AHB Prescaler     = 1
  *         APB1 Prescaler   = 4
  *         APB2 Prescaler   = 2
  *         PLL_M            = 25
  *         PLL_N            = 336
  *         PLL_P            = 2
  *         PLL_Q            = 7
  *         Flash Latency    = 5
  */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        while (1) {}
    }

    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        while (1) {}
    }

    if (HAL_GetREVID() == 0x1001) {
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
}
