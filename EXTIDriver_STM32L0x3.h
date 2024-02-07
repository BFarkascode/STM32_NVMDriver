/*
 *
 *  Created on: Oct 24, 2023
 *  Project: STM32_NVMDriver
 *  File: EXTIDriver_STM32L0x3.h
 *  Author: BalazsFarkas
 *  Processor: STM32L053R8
 *  Compiler: ARM-GCC (STM32 IDE)
 *  Program version: 1.0
 *  Program description: N/A
 *  Hardware description/pin distribution: N/A
 *  Modified from: N/A
 *  Change history: N/A
 *
 *      This is a driver for external interrupts.
 *      It is followed by the definition of the callback function of each EXTI.
 *
 */

#ifndef INC_EXTIDRIVER_CUSTOM_H_
#define INC_EXTIDRIVER_CUSTOM_H_

#include "stm32l053xx.h"											//device specific header file for registers

//LOCAL CONSTANT

//LOCAL VARIABLE

//EXTERNAL VARIABLE
extern uint32_t Data_buf [16];
extern uint32_t toggle_value1;
extern uint32_t toggle_value2;

//FUNCTION PROTOTYPES

void EXTIInit(void);

#endif /* INC_EXTIDRIVER_CUSTOM_H_ */
