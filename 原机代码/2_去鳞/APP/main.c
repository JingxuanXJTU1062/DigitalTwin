#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include <stdio.h>
#include "ttp223.h"
uint8_t flag = 1;
/**
	*	@brief		MAIN
	*	@param		
	*	@retval		
	*/
int main(void)
{
	board_init();
	
	Motor_Init();
	TTP223_Init();
	delay_ms(3000);

	X_V5_En_Control(1, 1, 0);
	delay_ms(100);
	X_V5_En_Control(2, 1, 0);
	delay_ms(100);
	X_V5_En_Control(3, 1, 0);
	delay_ms(100);
	X_V5_En_Control(4, 1, 0);
	delay_ms(100);
	X_V5_En_Control(5, 1, 0);
	delay_ms(100);

	while(1)
	{

		
		if(TTP223_GetTouchFlag() && flag)
        {
            TTP223_ClearTouchFlag();
			X_V5_Vel_Control(1, 0, 65535, 300, 0);
			delay_ms(200);
			X_V5_Vel_Control(2, 0, 65535, 2000, 0);
			delay_ms(200);
			X_V5_Vel_Control(3, 1, 65535, 10000, 0);
			delay_ms(200);
			X_V5_Vel_Control(4, 0, 65535, 20000, 0);
			delay_ms(200);
			X_V5_Vel_Control(5, 1, 65535, 2000, 0);
			delay_ms(200);
			Motor1_SetSpeed(20);
			delay_ms(10);
			Motor2_SetSpeed(-20);
			flag = 0;
			
        }
		delay_ms(100);

	}
}
