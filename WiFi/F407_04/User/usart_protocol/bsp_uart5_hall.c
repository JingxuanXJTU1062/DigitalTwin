/**
  ******************************************************************************
  * @file    bsp_uart5_hall.c
  * @brief   UART5 driver for F407_04 HAL version - 接 F103chuliao05 (出料)
  *
  *  Pin mapping:
 *    UART5  (PC12=TX, PD2=RX)   <- F103chuliao05 (12B 0x05 + optional 16B 0x03)
  *                                     仅用 PD2 (RX), PC12 悬空
  *
  *  接收策略 (与 UART4/USART2 一致):
  *    1. HAL_UART_Init -> HAL_UART_MspInit 配置 GPIO + NVIC
  *    2. 直接开启 RXNE 中断
  *    3. UART5_IRQHandler 读 SR/DR 并把字节推入协议环形队列
  ******************************************************************************
  */
#include "bsp_uart5_hall.h"
#include "./usart_protocol/usart_protocol.h"

UART_HandleTypeDef huart5;   /* UART5  -> F103chuliao05 (出料) */

volatile uint32_t g_uart5_hall_irq_count = 0;
volatile uint32_t g_uart5_hall_error_count = 0;
volatile uint32_t g_uart5_hall_sr = 0;
volatile uint32_t g_uart5_hall_cr1 = 0;
volatile uint32_t g_uart5_hall_brr = 0;
volatile uint32_t g_uart5_hall_pd2_mode = 0;
volatile uint32_t g_uart5_hall_pd2_af = 0;
volatile uint8_t  g_uart5_hall_last_byte = 0;
volatile HAL_StatusTypeDef g_uart5_hall_init_status = HAL_ERROR;
volatile HAL_StatusTypeDef g_uart5_hall_rx_status = HAL_ERROR;

void UART5_HALL_Config(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint32_t pclk1;

    /* 清除调试重启或旧固件遗留的 UART5 状态。 */
    __HAL_RCC_UART5_FORCE_RESET();
    __HAL_RCC_UART5_RELEASE_RESET();

    huart5.Instance          = UART5;
    huart5.Init.BaudRate     = 115200;
    huart5.Init.WordLength   = UART_WORDLENGTH_8B;
    huart5.Init.StopBits     = UART_STOPBITS_1;
    huart5.Init.Parity       = UART_PARITY_NONE;
    huart5.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart5.Init.Mode         = UART_MODE_RX;       /* 单向, 只接收 */
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;

    g_uart5_hall_init_status = HAL_UART_Init(&huart5);
    if (g_uart5_hall_init_status != HAL_OK) {
        return;
    }

    /* 显式重写 PD2/AF8，避免其他板级初始化模板遗留配置。 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* 显式配置 UART5 寄存器：115200, 8-N-1, RX only, oversampling 16。 */
    pclk1 = HAL_RCC_GetPCLK1Freq();
    UART5->CR1 = 0U;
    UART5->CR2 = 0U;
    UART5->CR3 = 0U;
    UART5->BRR = (pclk1 + (115200U / 2U)) / 115200U;

    /* 按 SR -> DR 顺序清理可能的历史 RXNE/ORE/FE/NE。 */
    g_uart5_hall_sr = huart5.Instance->SR;
    g_uart5_hall_last_byte = (uint8_t)huart5.Instance->DR;

    UART5->CR3 = USART_CR3_EIE;
    UART5->CR1 = USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    HAL_NVIC_ClearPendingIRQ(UART5_IRQn);
    HAL_NVIC_SetPriority(UART5_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(UART5_IRQn);

    g_uart5_hall_cr1 = UART5->CR1;
    g_uart5_hall_brr = UART5->BRR;
    g_uart5_hall_pd2_mode = (GPIOD->MODER >> (2U * 2U)) & 0x3U;
    g_uart5_hall_pd2_af = (GPIOD->AFR[0] >> (2U * 4U)) & 0xFU;
    g_uart5_hall_rx_status = HAL_OK;
}

/**
  * @brief  处理 UART5 当前待接收字节/错误状态。
  * @note   由 UART5 IRQ 和主循环轮询共用，两条路径不重复读 DR。
  * @retval None
  */
void UART5_HALL_IRQHandler(void)
{
    uint32_t sr = UART5->SR;
    uint8_t byte;

    g_uart5_hall_sr = sr;

    if ((sr & USART_SR_RXNE) != 0U) {
        byte = (uint8_t)(UART5->DR & 0xFFU);
        g_uart5_hall_last_byte = byte;
        g_uart5_hall_irq_count++;
        Protocol_ParseByte(UART_IDX_UART5_HALL, byte);
    } else if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
        byte = (uint8_t)(UART5->DR & 0xFFU);
        g_uart5_hall_last_byte = byte;
    }

    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
        g_uart5_hall_error_count++;
    }
}

/**
  * @brief  主循环轮询兜底；即使 NVIC/向量配置异常也可读取 RXNE。
  * @retval None
  */
void UART5_HALL_Poll(void)
{
    if ((UART5->SR & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE |
                      USART_SR_NE | USART_SR_PE)) != 0U) {
        UART5_HALL_IRQHandler();
    }
}
