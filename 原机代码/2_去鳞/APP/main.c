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
    X_V5_Stop_Now(2, 0);
    X_V5_Stop_Now(3, 0);
    X_V5_Stop_Now(4, 0);
    X_V5_Stop_Now(5, 0);
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
    uint8_t last_control_state = 2U;
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
        if (control_state != last_control_state) {
            if (control_state) Descale_Start();
            else Descale_Stop();
            last_control_state = control_state;
        }
        delay_ms(100);
    }
}
