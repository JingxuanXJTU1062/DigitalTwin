#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include <stdio.h>
#include <string.h>
#include "ttp223.h"
#include "PressureSensor.h"

uint8_t flag = 1;

/* 执行动作函数 */
void Motor_Action(void)
{
    X_V5_Vel_Control(1, 1, 65535, 150, 0);
    delay_ms(200);

    X_V5_Vel_Control(2, 0, 65535, 150, 0);
    delay_ms(200);

    X_V5_Vel_Control(3, 0, 65535, 15000, 0);
    delay_ms(200);

    Motor1_SetSpeed(50);
    delay_ms(50);
}

/**
    * @brief        MAIN
    * @param
    * @retval
    */
int main(void)
{
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

    while(1)
    {
        /* 保留原来的触摸触发逻辑 */
        if(TTP223_GetTouchFlag())
        {
            TTP223_ClearTouchFlag();
            Motor_Action();
        }

        /* 新增串口触发逻辑 */
        if(rxFrameFlag)
        {
            rxFrameFlag = false;

            if(strncmp((char *)rxCmd, "MOTOR_START", 11) == 0)
            {
                Emm_V5_Pos_Control(1, 1, 100, 0, 25000, 2, 0);
            }

            memset((void *)rxCmd, 0, FIFO_SIZE);
            rxCount = 0;
        }

        delay_ms(10);
    }

}
