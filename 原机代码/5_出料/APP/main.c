#include "board.h"
#include "delay.h"
#include "usart.h"
#include <stdio.h>
#include "ttp223.h"
#include "PressureSensor.h"

#define PRESSURE_TRIGGER_VOLTAGE  0.035f
#define START_DELAY_TICKS         20U

int main(void)
{
    uint8_t last_enable_state = 2U;
    uint8_t pressure_armed = 1U;
    uint8_t start_delay_ticks = 0U;

    board_init();
    PressureSensor_Init();
    TTP223_Init();
    delay_ms(3000);

    while (1) {
        uint8_t enabled = TTP223_ReadState() ? 1U : 0U;

        if (!enabled) {
            if (last_enable_state != 0U) {
                printf("MOTOR_STOP\r\n");
            }
            start_delay_ticks = 0U;
            pressure_armed = 1U;
            last_enable_state = 0U;
            delay_ms(100);
            continue;
        }

        last_enable_state = 1U;
        {
            uint16_t pressure_raw = PressureSensor_ReadRawAverage(1);
            float pressure_voltage = ((float)pressure_raw * 3.3f) / 4095.0f;

            if (pressure_voltage >= PRESSURE_TRIGGER_VOLTAGE) {
                pressure_armed = 1U;
            }

            if (start_delay_ticks > 0U) {
                --start_delay_ticks;
                if (start_delay_ticks == 0U && TTP223_ReadState()) {
                    printf("MOTOR_START\r\n");
                }
            } else if (pressure_voltage < 0.035f && pressure_armed) {
                pressure_armed = 0U;
                start_delay_ticks = START_DELAY_TICKS;
            }
        }

        delay_ms(100);
    }
}
