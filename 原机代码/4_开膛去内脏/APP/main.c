#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include <string.h>
#include "ttp223.h"

static void Gutting_Stop(void)
{
    X_V5_Stop_Now(1, 0);
    X_V5_Stop_Now(2, 0);
    X_V5_Stop_Now(3, 0);
    Motor1_SetSpeed(0);
}

static uint8_t Gutting_Continue(void)
{
    if (TTP223_ReadState()) return 1U;
    Gutting_Stop();
    return 0U;
}

static void Gutting_Start(void)
{
    X_V5_Vel_Control(1, 1, 65535, 150, 0);
    delay_ms(200);
    if (!Gutting_Continue()) return;
    X_V5_Vel_Control(2, 0, 65535, 150, 0);
    delay_ms(200);
    if (!Gutting_Continue()) return;
    X_V5_Vel_Control(3, 0, 65535, 15000, 0);
    delay_ms(200);
    if (!Gutting_Continue()) return;
    Motor1_SetSpeed(50);
}

static uint8_t Command_Equals(const volatile uint8_t *command,
                              uint8_t length, const char *expected)
{
    size_t expected_length = strlen(expected);

    while (length > 0U &&
           (command[length - 1U] == '\r' || command[length - 1U] == '\n')) {
        --length;
    }
    return length == expected_length &&
           memcmp((const void *)command, expected, expected_length) == 0;
}

static void Process_Discharge_Command(void)
{
    if (!rxFrameFlag) return;

    rxFrameFlag = false;
    if (Command_Equals(rxCmd, rxCount, "MOTOR_START")) {
        Emm_V5_Pos_Control(1, 1, 100, 0, 25000, 2, 0);
    } else if (Command_Equals(rxCmd, rxCount, "MOTOR_STOP")) {
        Emm_V5_Stop_Now(1, 0);
    }
    memset((void *)rxCmd, 0, FIFO_SIZE);
    rxCount = 0;
}

int main(void)
{
    uint8_t last_control_state = 2U;

    board_init();
    Motor_Init();
    TTP223_Init();
    delay_ms(3000);

    Emm_V5_En_Control(1, 1, 0);
    delay_ms(100);
    X_V5_En_Control(1, 1, 0);
    delay_ms(100);
    X_V5_En_Control(3, 1, 0);
    delay_ms(100);
    X_V5_En_Control(2, 1, 0);
    delay_ms(100);

    while (1) {
        uint8_t control_state = TTP223_ReadState() ? 1U : 0U;
        if (control_state != last_control_state) {
            if (control_state) Gutting_Start();
            else Gutting_Stop();
            last_control_state = control_state;
        }

        Process_Discharge_Command();
        delay_ms(10);
    }
}
