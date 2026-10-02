/**
  ******************************************************************************
  * @file    bsp_debug_usart.c
  * @brief   USART1 + UART4 + USART3 (ESP8266) unified HAL driver
  *
  *  Assignments:
  *    USART1 (PA9/PA10) <- F103 chuliao05 (????, 12-byte frame)
  *    UART4  (PC10/PC11) <- F103 jinliao01 (????, 10-byte frame)
  *    USART3 (PB10/PB11) <- ESP8266 (????)
  *
  *  USART1 debug printf ???? (fputc ???), printf ????? USART1 ????.
  ******************************************************************************
  */
#include "./usart/bsp_debug_usart.h"
#include "./ESP8266/bsp_esp8266.h"
#include "./ESP8266/wifi_cmd.h"
#include "./ESP8266/esp8266_ipd.h"
#include "./ESP8266/esp8266_at_events.h"
#include "./usart_protocol/usart_protocol.h"
#include "./usart_protocol/bsp_uart4_usart1.h"
#include "./usart_protocol/bsp_uart5_hall.h"

UART_HandleTypeDef UartHandle;     /* USART1 (jinliao02 进料2: 霍尔+压力) */
UART_HandleTypeDef Uart3Handle;    /* USART3 (ESP8266) */
extern UART_HandleTypeDef huart2;  /* USART2 (F407HALL 0x03), defined in bsp_uart4_usart1.c */
extern UART_HandleTypeDef huart5;  /* UART5 (F103chuliao05 0x05), defined in bsp_uart5_hall.c */
extern volatile uint8_t ucTcpClosedFlag;  /* defined in bsp_esp8266_test.c */
volatile uint32_t g_esp8266_uart_rx_count = 0U;
volatile uint32_t g_esp8266_uart_idle_count = 0U;
volatile uint32_t g_esp8266_uart_ore_count = 0U;

/**
  * @brief  Unified MSP: handles USART1, USART3, UART4 GPIO + NVIC setup
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    if (huart->Instance == USART1) {
        /* USART1 GPIO config (PA9=TX, PA10=RX) */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART1 NVIC: priority 2, sub 1 */
        HAL_NVIC_SetPriority(USART1_IRQn, 2, 1);
        HAL_NVIC_EnableIRQ(USART1_IRQn);

        /* Enable RXNE + IDLE interrupts */
        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
        __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

    } else if (huart->Instance == USART3) {
        /* USART3 GPIO config (PB10=TX, PB11=RX) */
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* USART3 NVIC: priority 0, sub 0 (highest) */
        HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);

        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
        __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

    } else if (huart->Instance == UART4) {
        /* UART4 GPIO config (PC10=TX, PC11=RX) */
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* UART4 NVIC: priority 2, sub 0 */
        HAL_NVIC_SetPriority(UART4_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(UART4_IRQn);

        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    } else if (huart->Instance == USART2) {
        /* USART2 GPIO config (PA2=TX, PA3=RX) - F407HALL (0x03) */
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART2 NVIC: priority 2, sub 2 (与 UART4/USART1 区分避免抢断) */
        HAL_NVIC_SetPriority(USART2_IRQn, 2, 2);
        HAL_NVIC_EnableIRQ(USART2_IRQn);

        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    } else if (huart->Instance == UART5) {
        /* UART5 GPIO: PD2=RX (AF8), PC12=TX 不接 */
        __HAL_RCC_UART5_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* UART5 NVIC: priority 2, sub 3 (与 USART1/UART4/USART2 区分) */
        HAL_NVIC_SetPriority(UART5_IRQn, 2, 3);
        HAL_NVIC_EnableIRQ(UART5_IRQn);

        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    }
}

/**
  * @brief  DEBUG_USART (USART1) GPIO and mode config - 115200 8-N-1
  * @param  None
  * @retval None
  */
void DEBUG_USART_Config(void)
{
    UartHandle.Instance = USART1;
    UartHandle.Init.BaudRate     = 115200;
    UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
    UartHandle.Init.StopBits     = UART_STOPBITS_1;
    UartHandle.Init.Parity       = UART_PARITY_NONE;
    UartHandle.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    UartHandle.Init.Mode         = UART_MODE_TX_RX;
    UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&UartHandle);
}

/* ============== USART1 ISR (F103 chuliao05) - pure RXNE, no HAL_UART_Receive_IT ============== */
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_ORE) != RESET) {
        (void)USART1->SR;
        (void)USART1->DR;
    }
    if (__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_RXNE) != RESET) {
        uint8_t ucCh = (uint8_t)(USART1->DR & 0xFFu);
        Protocol_ParseByte(UART_IDX_USART1, ucCh);
    }
    if (__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_IDLE) != RESET) {
        (void)USART1->DR;
        (void)USART1->SR;
    }
}

/* ============== ESP8266 USART3 ISR (existing) ============== */
void USART3_IRQHandler(void)
{
    uint32_t status = Uart3Handle.Instance->SR;
    uint8_t byte_valid = 0U;
    uint8_t ucCh = 0U;

    /* One SR snapshot followed by at most one DR read.  Reading DR again in
     * the IDLE branch can silently consume the one-byte '>' CIPSEND prompt. */
    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        ucCh = (uint8_t)(Uart3Handle.Instance->DR & 0xFFU);
        if ((status & USART_SR_RXNE) != 0U) byte_valid = 1U;
        if ((status & USART_SR_ORE) != 0U) g_esp8266_uart_ore_count++;
    } else if ((status & USART_SR_IDLE) != 0U) {
        (void)Uart3Handle.Instance->DR;
    }

    if (byte_valid) {
        g_esp8266_uart_rx_count++;
        /* Binary +IPD payload enters the ADU assembler.  Only non-payload
         * AT text is fed to the O(1) CLOSED detector and response buffer. */
        if (!Esp8266Ipd_PushByte(ucCh)) {
            uint32_t closed_before = Esp8266Ipd_GetClosedEventCount();
            Esp8266AtEvents_PushByte(ucCh);
            Esp8266Ipd_PushControlByte(ucCh);
            if (Esp8266Ipd_GetClosedEventCount() != closed_before) {
                ucTcpClosedFlag = 1U;
            }
            if (strEsp8266_Fram_Record.InfBit.FramLength < (RX_BUF_MAX_LEN - 1)) {
                uint16_t pos = strEsp8266_Fram_Record.InfBit.FramLength++;
                strEsp8266_Fram_Record.Data_RX_BUF[pos] = ucCh;
                strEsp8266_Fram_Record.Data_RX_BUF[pos + 1U] = '\0';
            }
        }
    }

    if ((status & USART_SR_IDLE) != 0U) {
        g_esp8266_uart_idle_count++;
        strEsp8266_Fram_Record.InfBit.FramFinishFlag = 1;
    }
}

/* ============== UART4 ISR (F103 jinliao01) ============== */
extern UART_HandleTypeDef huart4;
void UART4_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) != RESET) {
        uint8_t ucCh = (uint8_t)(UART4->DR & (uint8_t)0xFF);
        Protocol_ParseByte(UART_IDX_UART4, ucCh);
    }
    /* 错误中断处理: 清 ORE */
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) != RESET) {
        (void)UART4->DR;
    }
}

/* ============== USART2 ISR (F407HALL 0x03) ============== */
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
        uint8_t ucCh = (uint8_t)(USART2->DR & (uint8_t)0xFF);
        Protocol_ParseByte(UART_IDX_USART2, ucCh);
    }
    /* 错误中断处理: 清 ORE */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
        (void)USART2->SR;
        (void)USART2->DR;
    }
}

/* ============== UART5 ISR (F103chuliao05 出料 0x05) ============== */
void UART5_IRQHandler(void)
{
    UART5_HALL_IRQHandler();
}

/* ============== ESP8266 UART3 DMA/Tx done callback ============== */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
}

/* ============== Send string (generic) ============== */
void Usart_SendString(uint8_t *str)
{
    uint32_t k = 0;
    do {
        HAL_UART_Transmit(&UartHandle, (uint8_t *)(str + k), 1, 1000);
        k++;
    } while (*(str + k) != '\0');
}

/**
  * @brief  fputc redirected to USART1 (debug printf - now disabled)
  * @note   USART1 is now used for F103 chuliao05. printf output is disabled.
  */
int fputc(int ch, FILE *f)
{
    (void)f;
    return ch;
}

int fgetc(FILE *f)
{
    (void)f;
    return -1;
}
