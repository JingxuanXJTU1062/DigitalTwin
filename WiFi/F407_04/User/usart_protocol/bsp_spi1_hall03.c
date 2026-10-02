#include "bsp_spi1_hall03.h"
#include <string.h>

SPI_HandleTypeDef hspi1;
volatile uint8_t g_spi1_init_ok = 0U;
volatile uint32_t g_spi1_rx_count = 0U;
volatile uint32_t g_spi1_rx_error_count = 0U;
volatile uint8_t g_spi1_last_frame[HALL03_SPI_FRAME_LEN] = {0U};

static uint8_t s_last_frame[HALL03_SPI_FRAME_LEN] = {0U};
static uint8_t s_frame_valid = 0U;

void SPI1_HALL03_Config(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* PA4 is software-controlled CS; PA5/PA6/PA7 are SPI1 SCK/MISO/MOSI. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10U;
    g_spi1_init_ok = (HAL_SPI_Init(&hspi1) == HAL_OK) ? 1U : 0U;
}

uint8_t SPI1_HALL03_ReadFrame(uint8_t *frame_buf)
{
    uint8_t dummy[HALL03_SPI_FRAME_LEN];
    HAL_StatusTypeDef status;
    if ((g_spi1_init_ok == 0U) || (frame_buf == NULL)) return 0U;
    memset(dummy, 0xFF, sizeof(dummy));
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi1, dummy, frame_buf,
                                     HALL03_SPI_FRAME_LEN, 20U);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    if (status == HAL_OK) {
        ++g_spi1_rx_count;
        memcpy(s_last_frame, frame_buf, HALL03_SPI_FRAME_LEN);
        memcpy((uint8_t *)g_spi1_last_frame, frame_buf, HALL03_SPI_FRAME_LEN);
        s_frame_valid = 1U;
        return 1U;
    }
    ++g_spi1_rx_error_count;
    if (s_frame_valid != 0U) memcpy(frame_buf, s_last_frame, HALL03_SPI_FRAME_LEN);
    return 0U;
}

uint8_t SPI1_HALL03_GetLastFrame(uint8_t *frame_buf)
{
    if ((frame_buf == NULL) || (s_frame_valid == 0U)) return 0U;
    memcpy(frame_buf, s_last_frame, HALL03_SPI_FRAME_LEN);
    return 1U;
}
