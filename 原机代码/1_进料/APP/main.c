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
	
	TTP223_Init();
	delay_ms(3000);

	Emm_V5_En_Control(1, 1, 0);
	delay_ms(100);

	Emm_V5_En_Control(3, 1, 0);
	delay_ms(100);
	

	X_V5_En_Control(2, 1, 0);
	delay_ms(100);

	while(1)
	{

		
		if(TTP223_GetTouchFlag() && flag)
        {
            TTP223_ClearTouchFlag();
			Emm_V5_Vel_Control(1,0,50,0,0);
			delay_ms(200);
			Emm_V5_Vel_Control(3,1,35,0,0);
			delay_ms(200);
			X_V5_Vel_Control(2, 1, 65535, 500, 0);
			delay_ms(200); 
			flag = 0;
			
        }
		delay_ms(100);

	}
}
