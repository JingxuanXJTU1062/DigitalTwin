#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include "ttp223.h"

static void Feed_Stop(void)
{
    Emm_V5_Stop_Now(1, 0);
    delay_ms(20);
    Emm_V5_Stop_Now(3, 0);
    delay_ms(20);
    X_V5_Stop_Now(2, 0);
}

static void Feed_Start(void)
{
    Emm_V5_Vel_Control(1, 0, 50, 0, 0);
    delay_ms(200);
    if (!TTP223_ReadState()) {
        Feed_Stop();
        return;
    }

    Emm_V5_Vel_Control(3, 1, 35, 0, 0);
    delay_ms(200);
    if (!TTP223_ReadState()) {
        Feed_Stop();
        return;
    }

    X_V5_Vel_Control(2, 1, 65535, 500, 0);
}

int main(void)
{
    uint8_t is_running = 0U;
    uint8_t stop_sent = 0U;

    board_init();
    TTP223_Init();
    delay_ms(3000);

    Emm_V5_En_Control(1, 1, 0);
    delay_ms(100);
    Emm_V5_En_Control(3, 1, 0);
    delay_ms(100);
    X_V5_En_Control(2, 1, 0);
    delay_ms(100);

    while (1) {
        uint8_t control_state = TTP223_ReadState() ? 1U : 0U;

        if (control_state) {
            stop_sent = 0U;
            if (!is_running) {
                Feed_Start();
                is_running = TTP223_ReadState() ? 1U : 0U;
            }
        } else {
            if (is_running || !stop_sent) {
                Feed_Stop();
                stop_sent = 1U;
            }
            is_running = 0U;
        }
        delay_ms(100);
    }
}
