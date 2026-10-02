/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt handlers for F407_04 Digital Twin Gateway
  ******************************************************************************
  */
#include "main.h"
#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"

/* Exception Handlers */

void NMI_Handler(void) {}
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void)  { while (1) {} }
void UsageFault_Handler(void){ while (1) {} }
void SVC_Handler(void)      {}
void DebugMon_Handler(void)  {}
void PendSV_Handler(void)    {}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ============== UART4 ISR (F103 jinliao01) ==============
 * HAL framework: UART4_IRQHandler is the actual NVIC entry.
 * Implementation is in bsp_debug_usart.c (USART3/UART4 share unified MSP). */
void UART4_IRQHandler(void);

/* ============== USART3 ISR (ESP8266) ==============
 * Implementation is in bsp_debug_usart.c */
void USART3_IRQHandler(void);

/* ============== USART1 ISR (F103 chuliao05) ==============
 * Implementation is in bsp_debug_usart.c */
void USART1_IRQHandler(void);
