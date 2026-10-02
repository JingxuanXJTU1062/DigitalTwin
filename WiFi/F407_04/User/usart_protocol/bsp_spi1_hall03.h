#ifndef __BSP_SPI1_HALL03_H
#define __BSP_SPI1_HALL03_H

#include "stm32f4xx_hal.h"

#define HALL03_SPI_FRAME_LEN 17U

void SPI1_HALL03_Config(void);
uint8_t SPI1_HALL03_ReadFrame(uint8_t *frame_buf);
uint8_t SPI1_HALL03_GetLastFrame(uint8_t *frame_buf);

extern SPI_HandleTypeDef hspi1;
extern volatile uint8_t g_spi1_init_ok;
extern volatile uint32_t g_spi1_rx_count;
extern volatile uint32_t g_spi1_rx_error_count;
extern volatile uint8_t g_spi1_last_frame[HALL03_SPI_FRAME_LEN];

#endif
