/**
  ******************************************************************************
  * @file    bsp_uart4_usart1.c
  * @brief   UART4 + USART2 driver for F407_04 HAL version
  *
  *  Pin mapping:
  *    UART4  (PC10=TX, PC11=RX)   <- F103 jinliao01 (进料, 10B XOR 帧 0x01)
  *    USART1 (PA9=TX, PA10=RX)    <- F103 jinliao02 (霍尔+压力, 12B XOR 帧 0x02)
  *                                     -> 由 bsp_debug_usart.c 的 DEBUG_USART_Config 初始化,
  *                                        GPIO/NVIC 在 HAL_UART_MspInit 中配
  *    USART2 (PA2=TX, PA3=RX)     <- F407HALL (3霍尔+2测距, 16B XOR 帧 0x03)
  *    USART3 (PB10/PB11)          <- ESP8266 (已有, 不冲突)
  *
  *  接收策略 (纯 RXNE 中断, 不用 HAL_UART_Receive_IT):
  *    1. HAL_UART_Init -> HAL_UART_MspInit 配置 GPIO + NVIC
  *    2. __HAL_UART_ENABLE_IT(RXNE)    每字节触发 ISR
  *    3. ISR 直接读 DR, 调 Protocol_ParseByte(idx, byte)
  *    4. ISR 不需要再次启动接收 (RXNE 中断是每当 RXNE 位 1 就触发)
  *
  *  原因: 之前用 HAL_UART_Receive_IT 但没实现 HAL_UART_RxCpltCallback
  *        导致只能收到 1 字节后不再接收 -> "长时间不更新"
  ******************************************************************************
  */
#include "bsp_uart4_usart1.h"
#include "./usart_protocol/usart_protocol.h"

UART_HandleTypeDef huart4;   /* UART4  -> F103 jinliao01 (进料) */
UART_HandleTypeDef huart2;   /* USART2 -> F407HALL (0x03) */

static void UART4_Init(void)
{
    huart4.Instance          = UART4;
    huart4.Init.BaudRate     = 115200;
    huart4.Init.WordLength   = UART_WORDLENGTH_8B;
    huart4.Init.StopBits     = UART_STOPBITS_1;
    huart4.Init.Parity       = UART_PARITY_NONE;
    huart4.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart4.Init.Mode         = UART_MODE_TX_RX;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart4);

    /* RXNE 中断, 每字节触发 UART4_IRQHandler */
    __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
}

static void USART2_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart2);

    /* RXNE 中断, 每字节触发 USART2_IRQHandler */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

void UART4_USART1_Config(void)
{
    UART4_Init();
    USART2_Init();
    /* USART1 由 bsp_debug_usart.c 的 DEBUG_USART_Config() 初始化,
     * HAL_UART_MspInit 会根据 Instance 分发到对应分支. */
}
