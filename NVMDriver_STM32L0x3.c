/*
 *  Created on: Oct 25, 2023
 *  Author: BalazsFarkas
 *  Project: STM32_NVMDriver
 *  Processor: STM32L053R8
 *  Program version: 1.0
 *  File: NVMDriver_STM32L0x3.c
 *  Change history:
 *
 * v.1.0
 * Below is a custom NVM driver.
 * NVM is not the same as FLASH, albeit all registers are called FLASH.
 * In some devices, writing to FLASH is blocked until the target is erased. With others, the result will be the bitwise OR of the original and the new word.
 * Erasing FLASH is at least 8 x 4 x 32 bit. It is not possible to erase just a word or a byte.
 *
 */

#include "NVMDriver_STM32L0x3.h"
#include "main.h"


//1)FLASH speed and interrupt initialisation
void NVM_Init (void){
	/**
	 * Apart from unlocking (see refman page 82) and then locking the NVM's control panel, we only do one thing here:
	 * 1) We set up the NVM driver's interrupts.
	 * 
	 * Note: the NVM control panel will be closed by even the simplest activity outside this function in order to protect the FLASH from accidental corruption. The control panel must be unlocked to allow FLASH operations!
	 *
	 * Function to set NVM functions (FLASH, EEPROM and Option bytes).
	 * We generally don't need to use this since FLASH is already properly initialised upon startup and we don't use EEPROM.
	 * Separate NVMs should be interacted with separately in code.
	 * Note: even though all registers are called FLASH, it is actually NVM and not just FLASH.
	 * Note: in EEPROM, a page and the word are the same size.
	 *
	 * 1)Unlock NVMs
	 * 2)Set speed and buffers
	 * 3)Set interrupts
	 * 4)Close the PELOCK
	 *
	 **/

	//1)
	FLASH->PEKEYR = 0x89ABCDEF;					//PEKEY1
	FLASH->PEKEYR = 0x02030405;					//PEKEY2
												//Note: NVM has a two step enable element to unlock the PECR register and put PELOCK to 0

	//2)
	//FLASH.ACR register modification comes here when necessary.
	//It sets up the speed, latency and the preread of the NVMs.
	//we don't use it


	//3)
	FLASH->PECR &= ~(1<<16);					//EOP interrupt disabled (EOPIE)
												//Note: if we do FLASH writing word by word, this interrupt will be mostly useless.
	FLASH->PECR |= (1<<17);						//Error interrupt enabled (ERRIE)
//	FLASH->PECR |= (1<<23);						//we would enable the NZDISABLE erase check (will only allow writing to FLASH if FLASH has been erased)
												//on L0xx devices, it doesn't seem to exist

	//4)
//	FLASH->OPTR = (0xB<<0);						//we switch to Level 1 protection using RDPROT bits.
												//Usually Level 1 protection is fine and should not be changed.
												//Note: writing 0xCC to the RDPORT puts Level 2 protection, which bricks the micro indefinitely!!!
	//5)
	FLASH->PECR |= (1<<0);						//we set PELOCK on the NVM to 1, locking it again for writing operations
}

//2)Erase a page of FLASH
void FLASHErase_Page(uint32_t flash_page_addr) {
	/** This function erases a full page of FLASH. A page consists of 8 rows of 4 words (128 bytes or 1 kbit).
	 * It is not possible to erase a smaller section of FLASH than a page.
	 *
	 * After unlocking the NVM control panel again, we do three things here:
	 * 1) We choose the FLASH memory as our workspace by unlocking it and then tell the control panel that we want to erase. Since we pick erasing, the FLASH address will be incremented automatically for one page's worth of addresses.
	 * 2) We choose the address we want to erase and then wait until the erasing is done. The erasing will be aligned to a full page so we can't erase from a random spot. __IO is a macro for "volatile".
	 * 3) We reset the flags and close the NVM control panel.
	 *
	 * To implement in practice:
	 * 1)Unlock the NVM control register PECR.
	 * 2)Unlock FLASH memory.
	 * 3)Remove readout protection (if necessary)
	 * 4)Choose the erase action. Pick the FLASH as the target of the operation.
	 * 5)Replace the word with the new one and wait until success flag is raised
	 * 6)Close NVM and add readout protection
	 *
	 * Note: writing 0xCC to the RDPORT bricks the micro indefinitely!!!
	 **/

	//1)
	FLASH->PEKEYR = 0x89ABCDEF;					//PEKEY1
	FLASH->PEKEYR = 0x02030405;					//PEKEY2

	//2)
	FLASH->PRGKEYR = 0x8C9DAEBF;				//RRGKEY1
	FLASH->PRGKEYR = 0x13141516;				//RRGKEY2

	//3)
//	FLASH->OPTR = (0xAA<<0);					//we switch to Level 0 protection using RDPROT bits, if necessary (unless the FLASH protection is changed manually, this should be fine as it is)

	//4)
	FLASH->PECR |= (1<<9);						//we ERASE
	FLASH->PECR |= (1<<3);						//we pick the FLASH for erasing

	//5)
	*(__IO uint32_t*)(flash_page_addr) = (uint32_t)0;		//value doesn't actually matter here, we are erasing

	while((FLASH->SR & (1<<0)) == (1<<0));		//we stay in the loop while the BSY flag is 1

	while(!(((FLASH->SR & (1<<1)) == (1<<1))));	//we stay in the loop while the EOP flag is not 1
	FLASH->SR |= (1<<1);						//we reset the EOP flag to 0 by writing 1 to it

	//6)
//	FLASH->OPTR = (0xBB<<0);					//we switch back to Level 1 protection using RDPROT bits
	FLASH->PECR |= (1<<0);						//we set PELOCK on the NVM to 1, locking it again for writing operations
}

//3)Write a word to a FLASH address
void FLASHUpd_Word(uint32_t flash_word_addr, uint32_t updated_flash_value) {
	/** This function writes a 32-bit word in the NVM.
	 * The code assumes that the target position is already empty. If it is not, the resulting word will be corrupted (a bitwise OR of the original value and the new one).
	 * On L0xx, there is no NOTZEROERR control to avoid this corruption.
	 *
	 * After unlocking the NVM control panel again, we do four things here:
	 * 1) We so an endian swap on the word we wish to write to the FLASH. This is necessary since the FLASH will do one automatically during the process.
	 * 2) We choose the FLASH memory as our workspace by unlocking it. We don't need to specify anything in the control panel. Of note, neither the FLASH address nor the data will be incremented automatically here.
	 * 2) We choose the address we want to write to and then wait for the writing to be done. __IO is again a macro for "volatile".
	 * 4) We reset the flags and close the NVM control panel.
	 *
	 *In practice:
	 * 1)We do an endian swap on the input data (the FLASH will publish the data in an endian inverted to the code)
	 * 		Note: if the transmitted machine code already had an endian swap, this step is not necessary.
	 * 2)Unlock the NVM control register PECR.
	 * 3)Unlock FLASH memory.
	 * 4)Remove readout protection (if necessary)
	 * 5)Replace the word with the new one and wait until success flag is raised
	 * 			Note: NOTZEROERR flag/interrupt may not be available on certain devices, meaning that data will be written to a target independent of what is already there.
	 * 			Note: if NOTZEROERR flag/interrupt is active, only if the target is empty are we allowed to write there.
	 * 6)Close NVM and add readout protection
	 *
	 * Note: writing is a bitwise "OR" operation. Target must be erased first (see FLASHErase_Page function).
	 * Note: the arriving byte sequence is LSB byte first, not MSB byte first. The machine code within the micro is flipped compared to what is loaded into it.
	 * Note: writing 0xCC to the RDPORT bricks the micro indefinitely!!!
	 **/

	//1)
#ifdef endian_swap
	uint32_t swapped_updated_flash_value = ((updated_flash_value >> 24) & 0xff) | 		// move byte 3 to byte 0
	                    ((updated_flash_value << 8) & 0xff0000) | 						// move byte 1 to byte 2
	                    ((updated_flash_value >> 8) & 0xff00) | 						// move byte 2 to byte 1
	                    ((updated_flash_value << 24) & 0xff000000); 					// byte 0 to byte 3
#endif

	//2)
	FLASH->PEKEYR = 0x89ABCDEF;					//PEKEY1
	FLASH->PEKEYR = 0x02030405;					//PEKEY2

	//3)
	FLASH->PRGKEYR = 0x8C9DAEBF;				//RRGKEY1
	FLASH->PRGKEYR = 0x13141516;				//RRGKEY2
												//Note: FLASH has a two step enable element to unlock writing to the FLASH
												//Note: PRGLOCK bits being 0 is a precondition for writing to FLASH
												//Note: PELOCK is already removed in step 1), using the PEKEY

	//4)
//	FLASH->OPTR = (0xAA<<0);					//we switch to Level 0 protection using RDPROT bits
												//in-application FLASH should be modified at Level1 readout protection, so likley no need to change that

	//5)
	*(__IO uint32_t*)(flash_word_addr) = updated_flash_value;

#ifdef endian_swap
	//*(__IO uint32_t*)(flash_word_addr) = swapped_updated_flash_value;
#endif

												//Note: the target area must be erased before writing to it, otherwise data gets corrupted

	while((FLASH->SR & (1<<0)) == (1<<0));		//we stay in the loop while the BSY flag is 1
	while(!(((FLASH->SR & (1<<1)) == (1<<1))));	//we stay in the loop while the EOP flag is not 1
	FLASH->SR |= (1<<1);						//we reset the EOP flag to 0 by writing 1 to it

	//6)
//	FLASH->OPTR = (0xBB<<0);					//we switch back to Level 1 protection using RDPROT bits
	FLASH->PECR |= (1<<0);						//we set PELOCK on the NVM to 1, locking it again for writing operations
}




//4)Write a half-page to a FLASH address
void FLASHUpd_HalfPage(uint32_t flash_half_page_addr) {
	/**
	 * The function MUST run in RAM, not in FLASH!!!!!!
	 * Call with the __RAM_FUNC attribute!!!!!
	 *
	 * Also, ALL IRQs must be disabled during the write process or we have a crash.
	 * 
	 *
	 * This function writes a sixteen 32-bit words in the NVM.
	 * The address of the action must align to a half page - first 6 bits of the first address must be 0.
	 * The code assumes that the target position is empty. If it is not, the resulting word will be corrupted (a bitwise OR of the original value and the new one).
	 * On L0xx, there is no NOTZEROERR control to avoid this corruption.
	 *
	 * Input is the address of the half page we have erased before. The buffer array is an extern called Data_buf and is half a page long.
	 *
	 * The process is very similar to the "erase", except that we pick the half-page FLASH programming in the control panel.
	 * Additionally, we need to disable and re-enable all IRQs.
	 * The FLASH memory address will be stepped by a half-page worht of addresses, but the data side will need to be called using a loop (here I used a for loop).
	 * Of note, I had difficulties using a pointer for the data matrix and reverted to using the matrix directly by the function.
	 * Additionally, we are using the function by relying on local variables. Stepping (half-page selection and page selection) is done externally. It must be remembered that we write a half page, but actually always need to process a full page due to the bulk erase.
	 *
	 * //Note: we remain within the same half-page on this level
	 *
	 * 1)Unlock the NVM control register PECR.
	 * 2)Unlock FLASH memory.
	 * 3)Remove readout protection (if necessary)
	 * 4)We pick FLASH programming at half-page.
	 * 5)Disable IRQs
	 * 6)Replace the word with the new one and wait until success flag is raised
	 * 			Note: NOTZEROERR flag/interrupt may not be available on certain devices, meaning that data will be written to a target independent of what is already there.
	 * 			Note: if NOTZEROERR flag/interrupt is active, only if the target is empty are we allowed to write there.
	 * 7)Close NVM and add readout protection
	 * 8)Enable IRQs
	 *
	 * Note: writing is a bitwise "OR" operation. Target must be erased first (see FLASHErase_Page function).
	 * Note: the arriving byte sequence is LSB byte first, not MSB byte first. The machine code within the micro is flipped compared to what is loaded into it.
	 * Note: writing 0xCC to the RDPORT bricks the micro indefinitely!!!
	 **/

	//1)
	FLASH->PEKEYR = 0x89ABCDEF;					//PEKEY1
	FLASH->PEKEYR = 0x02030405;					//PEKEY2

	//2)
	FLASH->PRGKEYR = 0x8C9DAEBF;				//RRGKEY1
	FLASH->PRGKEYR = 0x13141516;				//RRGKEY2
												//Note: FLASH has a two step enable element to unlock writing to the FLASH
												//Note: PRGLOCK bits being 0 is a precondition for writing to FLASH
												//Note: PELOCK is already removed in step 1), using the PEKEY

	//3)
//	FLASH->OPTR = (0xAA<<0);					//we switch to Level 0 protection using RDPROT bits
												//in-application FLASH should be modified at Level1 readout protection, so likley no need to change that

	//4)
	FLASH->PECR |= (1<<3);						//we pick the FLASH for programming (PRG)
	FLASH->PECR |= (1<<10);						//we pick the half-page programming mode (FPPRG)


	//5)
	__disable_irq();							//we disable all the IRQs
												//Note: apparently this was "forgotten" in the refman, but one must deactivate all IRQs before working with FLASH, otherwise the writing will be interrupted
												//Note: it actually makes complete sense...a pickle it is not mentioned whatsoever

	//6)
	for(uint8_t i = 0; i < 16; i++) {
		*(__IO uint32_t*)(flash_half_page_addr) = Data_buf[i];
												//Note: the half page address does not need to be changed (similar to the erasing command)
												//Note: we only need to step the pointer for the data we want to write into the FLASH
	}

	while((FLASH->SR & (1<<0)) == (1<<0));		//we stay in the loop while the BSY flag is 1
	while(!(((FLASH->SR & (1<<1)) == (1<<1))));	//we stay in the loop while the EOP flag is not 1
												//EOP will go HIGH only after the 16 words have been copied properly
	FLASH->SR |= (1<<1);						//we reset the EOP flag to 0 by writing 1 to it

	//7)
	FLASH->PECR &= ~(1<<3);						//we disable the FLASH for programming
	FLASH->PECR &= ~(1<<10);					//we disable the half-page programming mode
	FLASH->PECR |= (1<<0);						//we set PELOCK on the NVM to 1, locking it again for writing operations

	//8)
	__enable_irq();								//we re-enable the IRQs
}




//5)
//if we encounter an error during writing to the FLASH, the code stops working
void FLASH_IRQHandler(void){
	/**
	* Simple IRQ to detect errors. We only have errors to trigger the IRQ.
	**/
	printf("Memory error... \r\n");
	FLASH->SR |= (0x32F<<8);						//we reset all the error interrupt flags
	while(1);
}


//6)FLASH IRQ priority
	/**
	* Priority assignemnt for the IRQ.
	**/
void FLASHIRQPriorEnable(void) {
	NVIC_SetPriority(FLASH_IRQn, 1);
	NVIC_EnableIRQ(FLASH_IRQn);
}
