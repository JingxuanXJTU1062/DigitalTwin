#ifndef _VL53L0X_I2C_H
#define _VL53L0X_I2C_H

#include "stm32f10x.h"
#include "stm32f10x_i2c.h"

#define VL53_I2C_PORT                I2C1
#define VL53_I2C_CLK                 RCC_APB1Periph_I2C1
#define VL53_I2C_GPIO_PORT           GPIOB
#define VL53_I2C_GPIO_CLK            RCC_APB2Periph_GPIOB
#define VL53_I2C_SCL_PIN             GPIO_Pin_6
#define VL53_I2C_SDA_PIN             GPIO_Pin_7
#define VL53_I2C_SPEED               100000U
#define VL53_I2C_TIMEOUT             30000U

void i2c_init(void);
void i2c_delay(void);
void i2c_bus_recover(void);
uint8_t i2c_probe(uint8_t addr_8bit);
uint8_t i2c_write(uint8_t addr, uint8_t reg, uint32_t len, uint8_t *data);
uint8_t i2c_read(uint8_t addr, uint8_t reg, uint32_t len, uint8_t *buf);
uint16_t i2cGetErrorCounter(void);

/* 保留旧接口声明，避免工程内其他文件包含时报错；
 * 硬件 I2C 方案不再依赖它们。 */
uint8_t i2c_start(void);
void    i2c_stop(void);
void    i2c_send_byte(uint8_t byte);
uint8_t i2c_wait_ack(void);

#endif
