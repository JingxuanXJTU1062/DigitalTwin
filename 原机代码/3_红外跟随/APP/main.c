#include "board.h"
#include "delay.h"
#include "usart.h"
#include <stdio.h>
#include "gripper_control.h"
#include "vl53_app.h"
#include "Emm_V5.h"
#include "Motor.h"
#include "PWM.h"
#include "ttp223.h"

int main(void)
{
    uint8_t ir_last = 0;
    uint8_t ir_now = 0;

    board_init();  

    delay_ms(3000);

    Emm_V5_En_Control(1, true, false);
    delay_ms(100);

    Emm_V5_Origin_Modify_Params(1, true, 2, 0, 200, 8000, 100, 800, 60, false);
    delay_ms(500);

    Emm_V5_Origin_Trigger_Return(1, 2, 0);
    delay_ms(5000);
	Emm_V5_Pos_Control(1,1,200,0,25600,2,0);

    /*
     * 记录进入主循环前的红外初始状态。
     * 如果上电时已经是无遮挡，则不会立即触发；
     * 只有经历过“遮挡 -> 无遮挡”才触发。
     */
    ir_last = ir_sensor_blocked();

    while (1)
    {
        ir_now = ir_sensor_blocked();

        /*
         * 有遮挡时：不控制电机；
         * 只有从遮挡变成无遮挡时：触发一次位置控制。
         */
        if ((ir_last == 1u) && (ir_now == 0u))
        {
            delay_ms(20);   // 消抖

            if (ir_sensor_blocked() == 0u)
            {
                Emm_V5_Pos_Control(1,1,20,0,3200,2,0);
				delay_ms(5000); 
				Emm_V5_Pos_Control(1,0,20,0,3200,2,0);
            }
        }

        ir_last = ir_now;

        delay_ms(50);
    }



}

  
