#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h"

/**********************************************************
***	Emm_V5.0ջ
***	дߣZHANGDATOU
***	֧֣Ŵͷջŷ
***	Ա̣https://zhangdatou.taobao.com
***	CSDNͣhttp s://blog.csdn.net/zhangdatou666
***	qqȺ262438510
**********************************************************/

void nvic_init(void);
void clock_init(void);
void usart_init(void);
void board_init(void);

void ir_sensor_init(void);
uint8_t ir_sensor_blocked(void);

#endif
