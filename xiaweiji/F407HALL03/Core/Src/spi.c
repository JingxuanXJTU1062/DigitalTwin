#include "spi.h"

void MX_SPI1_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);

  SPI1->CR1 = 0U; /* slave, mode 0, 8-bit, MSB first, hardware NSS */
  SPI1->CR2 = SPI_CR2_RXNEIE | SPI_CR2_ERRIE;
  HAL_NVIC_SetPriority(SPI1_IRQn, 4U, 0U);
  HAL_NVIC_EnableIRQ(SPI1_IRQn);
  SPI1->CR1 |= SPI_CR1_SPE;
}
