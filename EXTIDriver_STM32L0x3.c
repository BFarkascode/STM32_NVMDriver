/*
 *
 *  Created on: Oct 25, 2023
 *  Project: STM32_NVMDriver
 *  File: EXTIDriver_STM32L0x3.c
 *  Author: BalazsFarkas
 *  Processor: STM32L053R8
 *  Compiler: ARM-GCC (STM32 IDE)
 *  Program version: 1.0
 *  Program description: N/A
 *  Hardware description/pin distribution: N/A
 *  Modified from: N/A
 *  Change history: N/A
 *
 *v.1.0
 *	Below is the definition and the setup of the EXTI channels.
 *	Callback function names are taken from the device startup file.
 *	Current version uses PC13 and EXTI13 to engage the blue push button on the L053R8 nucelo board.
 */


#include "EXTIDriver_STM32L0x3.h"

//1)We initialize the EXTIs
void EXTIInit(void){
	/*
	 * Here in this init function we set up the EXTIs following the refman.
	 * We first activate the input pins, then choose if we wish to have the trigger on the rising or falling edge of the input.
	 * With the EXTI done, the connected IRQ will always be called when the trigger is activated.
	 * The trigger's flag must be reset though at the end of the IRQ, otherwise the trigger is not removed and the IRQ remains activated non-stop.
	 *
	 * Practical implementation is:
	 *
	 * 1)We set the pins up and select the EXTI lines we intend to use
	 * 2)We enable the syscfg clocking
	 * 3)We enable the EXTI channels on the pins
	 * 4)We configure the channels
	 * 5)We enable the IRQ and set their priority
	 *
	 * */

	//1)
		//Note: below we use PC13 "blue oush button" and thus, EXTI13
	RCC->IOPENR |= (1<<2);						//PORTC clocking allowed
	GPIOC->MODER &= ~(1<<26);					//PC13 input - EXTI13
	GPIOC->MODER &= ~(1<<27);					//PC13 input - EXTI13
												//we use push-pull
	GPIOC->OSPEEDR |= (3<<26);					//PC13 very high speed
												//no pull resistor

	//2)
	RCC->APB2ENR |= (1<<0);						//SYSCFG enable is on the APB2 enable register

	//3)
	SYSCFG->EXTICR[4] |= (1<<5);				//since we want to use PC13, we write 0010b to the [7:4] position of the fourth element of the register array - SYSCFG_EXTICR4
												//Note: EXTICR is uint32_t volatile [4], not just a 32-bit register (similar to AFR)
	//4)
	EXTI->IMR |= (1<<13);						//EXTI13 IMR unmasked
												//we leave the request masked
	EXTI->RTSR &= ~(1<<5);						//we disable the rising edge
	EXTI->FTSR |= (1<<5);						//we enable the falling edge

	//5)
	NVIC_SetPriority(EXTI4_15_IRQn, 1);			//we set the interrupt priority as 1 so it will be lower than the already running DMA IRQ for the SPI
	NVIC_EnableIRQ(EXTI4_15_IRQn);
}



//2)We define the callback function
void EXTI4_15_IRQHandler (void) {
	/*
	 * Note: it is good practice to double check that which pins have activated the EXTI.
	 *
	 * 1)Check which pin activated the EXTI
	 * 2)Act according to the pin activated
	 * 3)Reset the EXTI
	 *
	 * */

	//1)
	if (EXTI->PR & (1<<13)) {

		//2)

		  //the address we wish to erase
		  uint32_t flash_page_addr = 0x0800C000;

		  //We check, which value we have been running with (delay values are stored at C014 and C030)
		  uint32_t* toggle_pointer = 0x0800C014;
		  toggle_value1 = *toggle_pointer;

		  if (toggle_value1 == 0x00DB23FA) {			//if we were at 2000 ms
			  toggle_value1 = 0x005B23FA;
			  toggle_value2 = 0x0018005B;
		  } else {										//if not
			  toggle_value1 = 0x00DB23FA;
			  toggle_value2 = 0x001800DB;
		  }

		  Data_buf [5] = toggle_value1;					//we update the machine code

		  Data_buf [12] = toggle_value2;

		  //When the trigger happens, we replace the timing of the blink depending on what the original value was.
		  //Since this modification occurs directly in FLASH, the result will be carried over even after unpowering the system and won't be lost.
		  //Note: the machine code below may not be right if the original stack is changed and thus the pointers within the machine code would be pointing at the wrong place.

		  //We erase the area where the Blink_custom is
		  FLASHErase_Page(flash_page_addr);

		  //We rewrite the Blink_custom machine code
		  //Note: the machine code below may change between code optimisation. One machine code may not work for another code.
		  //Note: machine code should always be changed bulk!!!

#ifdef word_by_word
		  //half-page write using individual words
		  //Note: this version is 16 times slower than using a half-page burst. On the upside, it is very reliable.

		  //The two values below are pointers within the machine code that will also need to be updated if we change between half-page burst and word-by-word writing.
		  Data_buf [7] = 0x23A0FF07;
		  Data_buf [13] = 0xFEFAF7F4;


		  uint32_t* Data_buf_ptr = Data_buf;
		  for(int i = 0; i < 16; i++) {					//copying is done word by word. We do need to loop for 16 to reach half a page
			FLASHUpd_Word(flash_page_addr, *Data_buf_ptr++);
			flash_page_addr = flash_page_addr + 4;		//we increment the address value by 4. Of not, flash_page_addr is constant, not an array!
		  }
#endif

//#ifdef half_page
		  //half-page full burst
		  Data_buf [7] = 0x23A0FEB7;
		  Data_buf [13] = 0xFEAAF7F4;
		  FLASHUpd_HalfPage(flash_page_addr);
//#endif


		  //3)
		  EXTI->PR |= (1<<13);						//we reset the IRQ connected to the EXTI13 by writing to the pending bit
	}
}
