#include "board.h"
#include "delay.h"
#include "usart.h"
#include <stdio.h>
#include "ttp223.h"
#include "PressureSensor.h"

#define PRESSURE_TRIGGER_ENABLED  1U
#define PRESSURE_TRIGGER_VOLTAGE  0.035f
#define START_DELAY_TICKS         20U

int main(void)
{
#if PRESSURE_TRIGGER_ENABLED
    uint8_t pressure_armed = 1U;
    uint8_t pressure_active = 0U;
    uint8_t start_delay_ticks = 0U;
#endif
    uint8_t last_flag_state = 2U;

    board_init();
#if PRESSURE_TRIGGER_ENABLED
    PressureSensor_Init();
#endif
    TTP223_Init();
    delay_ms(3000);

    while (1) {
        uint8_t flag_request = TTP223_ReadState() ? 1U : 0U;
#if PRESSURE_TRIGGER_ENABLED
        uint8_t pressure_event = 0U;

        {
            uint16_t pressure_raw = PressureSensor_ReadRawAverage(1);
            float pressure_voltage = ((float)pressure_raw * 3.3f) / 4095.0f;

            if (pressure_voltage < PRESSURE_TRIGGER_VOLTAGE) {
                pressure_armed = 1U;
                pressure_active = 0U;
                start_delay_ticks = 0U;
            } else if (pressure_armed) {
                if (start_delay_ticks < START_DELAY_TICKS) {
                    ++start_delay_ticks;
                }
                if (start_delay_ticks >= START_DELAY_TICKS) {
                    pressure_armed = 0U;
                    pressure_active = 1U;
                    pressure_event = 1U;
                }
            } else {
                pressure_active = 1U;
            }
        }
#endif

        if (flag_request) {
            if (last_flag_state != 1U) {
                printf("MOTOR_RUN\r\n");
            }
        } else if (last_flag_state == 1U) {
#if PRESSURE_TRIGGER_ENABLED
            if (pressure_active) {
                printf("MOTOR_START\r\n");
            } else {
                printf("MOTOR_STOP\r\n");
            }
#else
            printf("MOTOR_STOP\r\n");
#endif
#if PRESSURE_TRIGGER_ENABLED
        } else if (pressure_event) {
            printf("MOTOR_START\r\n");
        } else if (last_flag_state == 2U && !pressure_active) {
            printf("MOTOR_STOP\r\n");
#else
        } else if (last_flag_state != 0U) {
            printf("MOTOR_STOP\r\n");
#endif
        }
        last_flag_state = flag_request;

        delay_ms(100);
    }
}
