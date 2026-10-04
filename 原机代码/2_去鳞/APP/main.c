#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include "ttp223.h"

static void Descale_Stop(void)
{
    X_V5_Stop_Now(1, 0);
    delay_ms(20);
    X_V5_Stop_Now(2, 0);
    delay_ms(20);
    X_V5_Stop_Now(3, 0);
    delay_ms(20);
    X_V5_Stop_Now(4, 0);
    delay_ms(20);
    X_V5_Stop_Now(5, 0);
    delay_ms(20);
    Motor1_SetSpeed(0);
    Motor2_SetSpeed(0);
}

static uint8_t Descale_Continue(void)
{
    if (TTP223_ReadState()) return 1U;
    Descale_Stop();
    return 0U;
}

static void Descale_Start(void)
{
    X_V5_Vel_Control(1, 0, 65535, 300, 0);
    delay_ms(200);
    if (!Descale_Continue()) return;
    X_V5_Vel_Control(2, 0, 65535, 2000, 0);
    delay_ms(200);
    if (!Descale_Continue()) return;
    X_V5_Vel_Control(3, 1, 65535, 10000, 0);
    delay_ms(200);
    if (!Descale_Continue()) return;
    X_V5_Vel_Control(4, 0, 65535, 20000, 0);
    delay_ms(200);
    if (!Descale_Continue()) return;
    X_V5_Vel_Control(5, 1, 65535, 2000, 0);
    delay_ms(200);
    if (!Descale_Continue()) return;
    Motor1_SetSpeed(20);
    delay_ms(10);
    if (!Descale_Continue()) return;
    Motor2_SetSpeed(-20);
}

int main(void)
{
    uint8_t is_running = 0U;
    uint8_t stop_sent = 0U;
    uint8_t address;

    board_init();
    Motor_Init();
    TTP223_Init();
    delay_ms(3000);

    for (address = 1; address <= 5; ++address) {
        X_V5_En_Control(address, 1, 0);
        delay_ms(100);
    }

    while (1) {
        uint8_t control_state = TTP223_ReadState() ? 1U : 0U;
        if (control_state) {
            stop_sent = 0U;
            if (!is_running) {
                Descale_Start();
                is_running = TTP223_ReadState() ? 1U : 0U;
            }
        } else {
            if (is_running || !stop_sent) {
                Descale_Stop();
                stop_sent = 1U;
            }
            is_running = 0U;
        }
        delay_ms(100);
    }
}
