#ifndef BSP_I2C3_HALL_H
#define BSP_I2C3_HALL_H
#include "stm32f4xx_hal.h"
#define HALL_I2C_ADDRESS (0x08U << 1U)
#define HALL_I2C_FRAME_LEN 16U
extern I2C_HandleTypeDef hi2c3_hall;
void I2C3_HALL_Config(void);
uint8_t I2C3_HALL_ReadFrame(uint8_t frame[HALL_I2C_FRAME_LEN]);
#endif
