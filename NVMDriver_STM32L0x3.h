/*
 *
 *  Created on: Oct 25, 2023
 *  Project: STM32_NVMDriver
 *  File: NVMDriver_STM32L0x3.h
 *  Author: BalazsFarkas
 *  Processor: STM32L053R8
 *  Compiler: ARM-GCC (STM32 IDE)
 *  Program version: 1.0
 *  Program description: N/A
 *  Hardware description/pin distribution: N/A
 *  Modified from: N/A
 *  Change history: N/A
 */

#ifndef INC_NVMDRIVER_STM32L0x3_CUSTOM_H_
#define INC_NVMDRIVER_STM32L0x3_CUSTOM_H_

#include "stdint.h"
#include "stm32l053xx.h"

#include "EXTIDriver_STM32L0x3.h"

//LOCAL CONSTANT

//LOCAL VARIABLE

//EXTERNAL VARIABLE
extern uint32_t Data_buf [16];

//FUNCTION PROTOTYPES
void NVM_Init (void);
void FLASHErase_Page(uint32_t flash_page_addr);
void FLASHUpd_Word(uint32_t flash_word_addr, uint32_t updated_flash_value);
void FLASHIRQPriorEnable(void);

__attribute__((section(".RamFunc"))) void FLASHUpd_HalfPage(uint32_t flash_page_addr);
					//Note: this function MUST run from RAM, not FLASH!

#endif /* INC_NVMDRIVER_STM32L0x3_CUSTOM_H_ */

