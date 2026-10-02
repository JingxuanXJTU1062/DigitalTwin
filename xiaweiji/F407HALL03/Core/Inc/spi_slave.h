#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include "hall03_frame.h"

extern volatile uint8_t s_tx_buf[HALL03_FRAME_SIZE];
extern volatile uint32_t g_spi1_tx_count;
extern volatile uint32_t g_spi1_error_count;
extern volatile uint32_t g_spi1_last_error;

void SPI_Slave_Init(void);
void SPI_Slave_BuildFrame(uint32_t omega1_mrad, uint32_t omega2_mrad,
                          uint16_t dist1_mm, uint16_t dist2_mm,
                          uint16_t dist3_mm, uint16_t range_status);
void SPI_Slave_Poll(void);
void SPI_Slave_IRQHandler(void);

#endif
