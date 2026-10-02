#include "spi_slave.h"
#include "main.h"

volatile uint8_t s_tx_buf[HALL03_FRAME_SIZE];
volatile uint32_t g_spi1_tx_count = 0U;
volatile uint32_t g_spi1_error_count = 0U;
volatile uint32_t g_spi1_last_error = 0U;

static uint8_t s_tx_snapshot[HALL03_FRAME_SIZE];
static volatile uint8_t s_index = 0U;

static void SPI_Slave_LoadSnapshot(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) s_tx_snapshot[i] = s_tx_buf[i];
  __set_PRIMASK(primask);
}

void SPI_Slave_Init(void)
{
  uint8_t initial[HALL03_FRAME_SIZE];
  Hall03Frame_Build(initial, 0U, 0U, 0U, 0U, 0U, 0U);
  for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) s_tx_buf[i] = initial[i];
  SPI_Slave_LoadSnapshot();
  s_index = 0U;
  *((__IO uint8_t *)&SPI1->DR) = s_tx_snapshot[0];
}

void SPI_Slave_BuildFrame(uint32_t o1, uint32_t o2, uint16_t d1, uint16_t d2,
                          uint16_t d3, uint16_t rs)
{
  uint8_t next[HALL03_FRAME_SIZE];
  Hall03Frame_Build(next, o1, o2, d1, d2, d3, rs);
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) s_tx_buf[i] = next[i];
  __set_PRIMASK(primask);
}

void SPI_Slave_IRQHandler(void)
{
  uint32_t sr = SPI1->SR;
  if ((sr & SPI_SR_RXNE) != 0U) {
    (void)(*((__IO uint8_t *)&SPI1->DR));
    ++s_index;
    if (s_index >= HALL03_FRAME_SIZE) {
      ++g_spi1_tx_count;
      SPI_Slave_LoadSnapshot();
      s_index = 0U;
    }
    *((__IO uint8_t *)&SPI1->DR) = s_tx_snapshot[s_index];
  }
  if ((sr & (SPI_SR_OVR | SPI_SR_MODF)) != 0U) {
    volatile uint32_t clear;
    clear = SPI1->DR;
    clear = SPI1->SR;
    (void)clear;
    ++g_spi1_error_count;
    g_spi1_last_error = sr;
    s_index = 0U;
    SPI_Slave_LoadSnapshot();
    *((__IO uint8_t *)&SPI1->DR) = s_tx_snapshot[0];
  }
}

void SPI_Slave_Poll(void)
{
  /* Interrupt-driven. Kept as a stable main-loop hook for diagnostics. */
}
