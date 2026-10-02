#include "board.h"
#include "delay.h"
#include "usart.h"
#include "Emm_V5.h"
#include "X_V5.h"
#include "Motor.h"
#include "PWM.h"
#include <stdio.h>
#include "ttp223.h"
#include "PressureSensor.h"
uint8_t flag = 1;
/**
	*	@brief		MAIN
	*	@param		
	*	@retval		
	*/
int main(void)
{
	uint16_t pressure_raw = 0;
	float pressure_voltage = 0.0f;

	board_init();
	PressureSensor_Init();
	

	delay_ms(3000);
	

	uint8_t pressure_pressed_last = 0;
	while(1)
	{
        pressure_raw = PressureSensor_ReadRawAverage(1);
		pressure_voltage = ((float)pressure_raw * 3.3f) / 4095.0f;
//		printf("Pressure Raw=%u, Voltage=%.3fV\r\n", pressure_raw, pressure_voltage);
		if(pressure_voltage < 0.035f && pressure_pressed_last == 0)
			{
								
				pressure_pressed_last = 1;
				delay_ms(2000);
				printf("MOTOR_START\r\n");
				delay_ms(300);
			}

			// 松开后，允许下一次物体重新触发
			if(pressure_voltage >= 0.035f)
			{
				pressure_pressed_last = 0;
			}
		delay_ms(100);

	}
}
