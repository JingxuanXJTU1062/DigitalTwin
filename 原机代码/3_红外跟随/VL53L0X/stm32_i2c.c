#include "stm32_i2c.h"
#include "delay.h"

static uint16_t g_i2c_error_counter = 0;

static int i2c_wait_event(uint32_t event)
{
    uint32_t timeout = VL53_I2C_TIMEOUT;
    while (I2C_CheckEvent(VL53_I2C_PORT, event) != SUCCESS)
    {
        if (--timeout == 0U)
        {
            ++g_i2c_error_counter;
            return 1;
        }
    }
    return 0;
}

static void i2c_gpio_od_high_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(VL53_I2C_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = VL53_I2C_SCL_PIN | VL53_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(VL53_I2C_GPIO_PORT, &GPIO_InitStructure);

    GPIO_SetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SCL_PIN | VL53_I2C_SDA_PIN);
}

static void i2c_gpio_af_od_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(VL53_I2C_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = VL53_I2C_SCL_PIN | VL53_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(VL53_I2C_GPIO_PORT, &GPIO_InitStructure);
}

static void i2c_periph_deinit(void)
{
    I2C_Cmd(VL53_I2C_PORT, DISABLE);
    I2C_SoftwareResetCmd(VL53_I2C_PORT, ENABLE);
    delay_ms(1);
    I2C_SoftwareResetCmd(VL53_I2C_PORT, DISABLE);
    I2C_DeInit(VL53_I2C_PORT);
}

static void i2c_periph_init(void)
{
    I2C_InitTypeDef I2C_InitStructure;

    RCC_APB1PeriphClockCmd(VL53_I2C_CLK, ENABLE);
    i2c_periph_deinit();
    i2c_gpio_af_od_init();

    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = VL53_I2C_SPEED;

    I2C_Init(VL53_I2C_PORT, &I2C_InitStructure);
    I2C_Cmd(VL53_I2C_PORT, ENABLE);
    I2C_AcknowledgeConfig(VL53_I2C_PORT, ENABLE);
}

void i2c_delay(void)
{
    volatile uint32_t i = 80U;
    while (i--) { }
}

void i2c_bus_recover(void)
{
    uint8_t i;

    i2c_gpio_od_high_init();

    for (i = 0; i < 9U; i++)
    {
        GPIO_SetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SCL_PIN);
        i2c_delay();
        GPIO_ResetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SCL_PIN);
        i2c_delay();
    }

    GPIO_ResetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SDA_PIN);
    i2c_delay();
    GPIO_SetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SCL_PIN);
    i2c_delay();
    GPIO_SetBits(VL53_I2C_GPIO_PORT, VL53_I2C_SDA_PIN);
    i2c_delay();
}

void i2c_init(void)
{
    i2c_bus_recover();
    i2c_periph_init();
}

uint8_t i2c_probe(uint8_t addr_8bit)
{
    uint32_t timeout;

	timeout = VL53_I2C_TIMEOUT;
	while (I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_BUSY))
	{
		if (--timeout == 0U)
		{
        i2c_init();
        break;
		}
	}

    I2C_GenerateSTART(VL53_I2C_PORT, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT))
    {
        i2c_init();
        return 1;
    }

    I2C_Send7bitAddress(VL53_I2C_PORT, addr_8bit, I2C_Direction_Transmitter);
    timeout = VL53_I2C_TIMEOUT;
    while ((I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_ADDR) == RESET) &&
           (I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_AF) == RESET))
    {
        if (--timeout == 0U)
        {
            ++g_i2c_error_counter;
            I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
            i2c_init();
            return 1;
        }
    }

    if (I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_AF) != RESET)
    {
        I2C_ClearFlag(VL53_I2C_PORT, I2C_FLAG_AF);
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    (void)VL53_I2C_PORT->SR1;
    (void)VL53_I2C_PORT->SR2;
    I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
    return 0;
}

uint8_t i2c_write(uint8_t addr, uint8_t reg, uint32_t len, uint8_t *data)
{
    uint32_t i;
    uint32_t timeout = VL53_I2C_TIMEOUT;

    while (I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_BUSY))
    {
        if (--timeout == 0U)
        {
            i2c_init();
            return 1;
        }
    }

    I2C_GenerateSTART(VL53_I2C_PORT, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) return 1;

    I2C_Send7bitAddress(VL53_I2C_PORT, addr, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    I2C_SendData(VL53_I2C_PORT, reg);
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        I2C_SendData(VL53_I2C_PORT, data[i]);
        if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        {
            I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
            return 1;
        }
    }

    I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
    return 0;
}

uint8_t i2c_read(uint8_t addr, uint8_t reg, uint32_t len, uint8_t *buf)
{
    uint32_t timeout = VL53_I2C_TIMEOUT;

    if ((buf == 0) || (len == 0U)) return 1;

    while (I2C_GetFlagStatus(VL53_I2C_PORT, I2C_FLAG_BUSY))
    {
        if (--timeout == 0U)
        {
            i2c_init();
            return 1;
        }
    }

    I2C_AcknowledgeConfig(VL53_I2C_PORT, ENABLE);

    I2C_GenerateSTART(VL53_I2C_PORT, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) return 1;

    I2C_Send7bitAddress(VL53_I2C_PORT, addr, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    I2C_SendData(VL53_I2C_PORT, reg);
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    I2C_GenerateSTART(VL53_I2C_PORT, ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) return 1;

    I2C_Send7bitAddress(VL53_I2C_PORT, addr, I2C_Direction_Receiver);
    if (i2c_wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        return 1;
    }

    while (len)
    {
        if (len == 1U)
        {
            I2C_AcknowledgeConfig(VL53_I2C_PORT, DISABLE);
            I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
        }

        timeout = VL53_I2C_TIMEOUT;
        while (I2C_CheckEvent(VL53_I2C_PORT, I2C_EVENT_MASTER_BYTE_RECEIVED) != SUCCESS)
        {
            if (--timeout == 0U)
            {
                ++g_i2c_error_counter;
                I2C_GenerateSTOP(VL53_I2C_PORT, ENABLE);
                I2C_AcknowledgeConfig(VL53_I2C_PORT, ENABLE);
                i2c_init();
                return 1;
            }
        }

        *buf++ = I2C_ReceiveData(VL53_I2C_PORT);
        len--;
    }

    I2C_AcknowledgeConfig(VL53_I2C_PORT, ENABLE);
    return 0;
}

uint16_t i2cGetErrorCounter(void)
{
    return g_i2c_error_counter;
}

uint8_t i2c_start(void) { return 1; }
void i2c_stop(void) { }
void i2c_send_byte(uint8_t byte) { (void)byte; }
uint8_t i2c_wait_ack(void) { return 1; }
