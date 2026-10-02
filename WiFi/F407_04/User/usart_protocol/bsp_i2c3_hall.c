#include "bsp_i2c3_hall.h"
I2C_HandleTypeDef hi2c3_hall;
void I2C3_HALL_Config(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_I2C3_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_AF_OD; gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF4_I2C3;
    gpio.Pin = GPIO_PIN_8; HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_9; HAL_GPIO_Init(GPIOC, &gpio);
    hi2c3_hall.Instance = I2C3;
    hi2c3_hall.Init.ClockSpeed = 100000U; hi2c3_hall.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c3_hall.Init.OwnAddress1 = 0U; hi2c3_hall.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3_hall.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE; hi2c3_hall.Init.OwnAddress2 = 0U;
    hi2c3_hall.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE; hi2c3_hall.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    (void)HAL_I2C_Init(&hi2c3_hall);
}
uint8_t I2C3_HALL_ReadFrame(uint8_t frame[HALL_I2C_FRAME_LEN])
{
    return (HAL_I2C_Master_Receive(&hi2c3_hall, HALL_I2C_ADDRESS, frame,
                                  HALL_I2C_FRAME_LEN, 20U) == HAL_OK) ? 1U : 0U;
}
