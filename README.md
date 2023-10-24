# STM32_NVMDriver
## General description
This is a bare metal guide for implementing FLASH management for STM32L0xx.

First and foremost, it must be discussed what we will be doing and on what. There are multiple memory types in the mcu, such as RAM, FLASH, EEPROM…and even though the driver section in the reference manual (refman) talks about non-volatile memory (NVM) and call the registers "FLASH", we will be looking at the FLASH memory only. As such, even though the refman will be constantly pointing to NVMs in general, we will be talking about FLASH.

Questiona rises, why would we do so? Wouldn’t it be simpler to use something special and purpose-made like EEPROM – another NVM – to use for memory testing? Yes, indeed and I may do an EEPROM guide as well...albeit here, my intent is to dissipate some of the terror that comes with digging inside the FLASH of a mcu. After all, if something goes sideways, mucking inside a FLASH can lead to severe adverse outcomes (even bricking) while messing up the EEPROM would not do any particular harm.

For all intents and purposes, modifying the FLASH of an mcu is doing brain surgery on it. Having an mcu modify ITSELF – which we will be doing here – is like conducting brain surgery on yourself! Pretty nasty with a rather low margin of failure. So, let's get to it!

I believe the best way to do such guide is to have a relatable example. Say, we have a simple Blinky function, and we want to change the blinking speed when we push a button. Depending on what abstraction layer or environment we were using, this usually means a few lines of code and a variable that needs to hold the value of the delay between blinks. Here, what happens in layman’s terms is that:
- upon startup, we “import” the Blink code into the mcu RAM from the mcu FLASH memory (where the Blink code is stored in some form of machine code after the mcu toolchain have compiled and uploaded it - one can actually visually see it using the "Memory" section within the STM32CubeIDE)
- we find the variable in this Blink code that holds the delay value
- we change the variable’s value when we push the button (we can poll a GPIO input or use an interrupt, whatever seems more reasonable)
- we enjoy our blinky
Nice and easy, simplest thing one can do with an mcu. But...

...let’s go ultra hard mode on this example! 
- Instead of changing a variable, let’s define the delay as a constant within the blink function.
- Rewrite the actual machine code stored in FLASH to change the blinking speed upon pushing a button.
Yes, by doing this, we do make our lives significantly more complicated. Nevertheless, we always will have a good reference point to reproduce with out hard mode coding, which is a good starting point when one has little to no idea, what he is doing.

Of note, before we take a dive-in, it is highly advised to get a sense for the memory location and memory types. I will touch upon them a bit below within the “particularities” section, but there won’t be a detailed explanation given on them. (This is a good place to start: http://www.gotw.ca/gotw/009.htm)
It is also very useful to forget about thinking in variables and instead start thinking in memory locations instead. Using variables is just a shortcut to a memory location where the mcu holds the value of the variable. If we know at which address the variable is, we can skip the middleman (the variable) and interact directly with the memory location instead. This is what pointers do...and no, they aren't easy to get a hang of. Unfortunately, the way how I got on terms with them is simply by using them. No amount of reading could help me get a hang of them.


## To read
The refman discusses a myriad of ways to optimize NVM use which we are not concerned about right now. Similarly, it discusses not just the FLASH, but also the option bits and the EEPROM which we aren’t looking into here either. As such, the absolutely relevant sections in the refman to write to the FLASH directly are (I am using the refman for the L0x3 here):
-	3.3.4 Writing and erasing the NVM: crucial information is shared here over how to unlock the FLASH. Mind, the FLASH is read-only whenever an app is being executed, thus this read only state must be removed by writing a set of values to a set of registers. One MUST follow these instructions very carefully.
-	3.7. Flash registers: a list of the registers to control the FLASH. This section will be used a LOT during the project. As a matter of fact, bare metal coding IS the application of this section in practice.
It is very much recommended to read the following three sections as well to know what never to do or how to slavage a messed up situation:
-	3.3.2 Dual-bank boot capacity: how to engage the embedded bootloader. STM32 comes with a gate-out-of-jail-free card in case something goes horribly wrong. One can use the embedded bootloader to bypass whatever mess we made and thus reset everything back to square one.
-	3.3.3 Reading the NVM: how we run the NVM. Generally good to know, albeit we don’t need to change anything from the standard setup.
-	3.4.1 Memory protection: good to know what NEVER to change unless we want to brick the mcu. Similar to the speed setup in 3.3.3., very useful to know, but we won’t be changing these.

## Particularities
How an mcu deals with data is that it loads the code one has written sequentially to the empty sections of memory, right after the vector table (vector table is like a phone book for the mcu: it tells it how to access certain basic functions for it to do its job. Again I will loo into those more in detail when discussing how to transition between a bootloader and an app. Check the bootloader repo!). As such, it very much depends on the code – as well as the compiler - where certain functions will end up in memory. Unless we don’t tell the compiler to specifically place a certain section of the code to a certain place, it will be placed randomly. This would make our hard-mode Blinky example pretty difficult to pull off…

### Linker file
So let’s put the Blink function of our code to a certain custom position in memory!
Okay, but there doesn’t seem to be a lot of option on it in our main.c file. Exactly!...Because it's not done in there or even in C!
We actually need to go to the linker file, stored in the root folder of the STM32 project with the extension “.ld”. (For this project, the linker file is provided as well.)
The linker file is used by the toolchain to assign memory locations and partitions. It is not written in c, but something called “link editor command language”. We can modify this linker file to have our own memory area/partition and then to force the compiler to move data into a section within that area. As such:

1) We add a new memory partition called APP_MEM by adding a line to the “memory definition” part in the ".ld" linker file just after the “RAM” and the “FLASH” areas. We define it to start from the origin 0x800c000 (48kbytes worth of address after the base memory of 0x8000000) and have a length of 16kbytes. We need to decrease the original length of the FLASH partition length from 64kbytes by the same length as APP_MEM is. Of note, the vector table (the mcu phone book) is – unless otherwise told so – will be stored at the memory bse address 0x8000000 thus the FLASH partition must start from there! (To not do so, again, see the bootloader project.)

```
/* Memories definition */
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 8K
  FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 48K
  APP_MEM (rx)		: ORIGIN = 0x800C000,   LENGTH = 16K				/*we define a designated section to manipulate, otherwise we might mess up the app*/
}
```

2) We define the app_section and then place it within the APP_MEM partition. We place the Blink_custom function we will be defining in the main.c into this section. The syntax is particular and should be followed to the letter (including the position of the definition within the linker!), otherwise the compiler might optimize out the memory section. The “KEEP” keyword – which works at “volatile” in c - is particularly useful. ALIGN(4) is a Cortex M architecture demand to align the location to 4 bytes. All in all, we tell the linker to put this section into the APP_MEM area and we finish the definition with a memory overflow check, albeit this is not necessary.

```
/*APP mem section definition*/
  .app_section :														/*memory section in the FLASH memory block to store the APP*/
  {
  	. = ALIGN(4);
  	__app_section_start__ = .;
  	KEEP(*(.app_section*))												/*KEEP must be used or the section might be removed by the compiler if not used*/
  	KEEP(*(.text.Blink_custom))											/*we put the custom Blink function into the app memory for easier access*/
  	__app_section_end__ = .;
  } > APP_MEM
  
/* check for memory overflow in the APP*/
ASSERT(LENGTH(APP_MEM) >= (__app_section_end__ - __app_section_start__), "APP memory has overflowed!")
```

One more part to look at wihtin the linker is the “initialized data section” or the “data” sections. This section is within RAM, albeit it is copied over from the FLASH. This is the “running” part of the code, the one that is moved over from the FLASH in order to execute the code. While RAM is actively engaged and used during code execution, FLASH may not be – or downright must not be as we will see in the half-page burst mode for the FLASH. We can move functions or variables over from FLASH and store them in RAM should we choose to. (We will choose to because we need to.) In order to place the function into RAM, the line

```
__attribute__((section(".RamFunc")))
```
needs to be added before the prototype of the function. The proposed “__RAM_FUNC” attribute assignment did not work for me on the STM32L0xx but should be fine on a BluePill.

For more on linker files, I recommend checking this out: Writing Linker Script for STM32 (Arm Cortex M3)🛠️ | by Rohit Nimkar | Medium

### Memory replacement
The FLASH must always be erased prior to writing to it. That’s because the NVM management system within the mcu does not overwrite the existing value within the memory position but instead uses a bitwise “OR” on the location. This is a simple addition if we have 0x0 in the memory, but if we are to write, say, 0x80 to a location where we had 0x1 before, the result of the memory write will be 0x81. This "bug" is usually detected by an error flag within the NVM, but the L0xx series does not seem to have this detection (I know the BluePill does).

FLASH cannot be erased byte-by-byte either. The smallest section one can erase is a page of memory, which is 128 bytes of data. As such, for any kind of FLASH update, the smallest section to replace is 32 words (32 times 4 bytes).

FLASH has a word-by-word (4 bytes) and a half-page write capability. Half-page is more sensitive due to the bursting of data but is around 10 times faster than writing word-by-word. Nothing smaller than 4 bytes can be written.

Lastly, one must be very cautious about the endian of the information that is funnelled into the FLASH. The writing process swaps the endian of the 4 bytes, where the 0th byte will be written to the last position (and vica versa) and the 1st will be swapped by the 2nd. As such, all machine code need to be pre-swapped before being fed into the NVM driver.

### Blocking you MUST have!
Writing to FLASH takes at least 3.2 ms, independent of which process is being executed (see refman page 94). During the time of writing into FLASH, the FLASH MUST NOT be used by any other function, IRQ, process, or anything really...otherwise the mcu will freeze. As such, it must be ensured that whenever we are working with the FLASH, all other potential activities are halted, suspended or cancelled. (In my experience, this must be applied to non-user-defined IRQs as well such as certain systicks.) Word-by-word writing can be made blocking easily to deal with this problem, but half-page needs special care (we will discuss this within the code itself).

Half-page also must be run from RAM directly following what the refman suggest (refman page 82, example code A.3.10).

## User guide
The driver codes provided are self-containing.

The main.c shows working examples for the driver where we define a custom Blink function that is placed by the linker to a specific memory location (0x800C000). We then replace this Blink function (it is half a page long exactly) when we push a button. The difference between the versions of the Blnik function will be the blinking speed.

For the sake of simplicity, the push button is connected directly to then external interrupt (EXTI). The code replacement functions are placed within the IRQ itself (which is not a good practice, but since we aren't doing anything else within the code, we don't care). The interrupt is activated on the L053r* nulceo board by pushing the blue button.

The EXTI and how it is engaged is not explained separately: it is a short read in the refman with a straight forward implementation.

On pushing the button, the blinking speed of the inbuilt LED should change (and then, unlike doing this by modifying a variable, retain that change upon reset) between 500 ms and 2000 ms.

Ideally, the difference in machine code between the two custom blink functions - the two LED blinking speeds - will only be two bytes, one at 0x800c014, the other at 0x800c030, the rest of the machine code remains the same. One can directly check memory positions after the project is built and the memory viewer in the STM32CubeIDE is opened. Thus we only need to update two bytes in FLASH to change the blinking speed. We change more due to the fact that we can't erase less than a page and we need to rewrite the part that is unnecessarily deleted.

Note: In reality, the machine code definining the function can change if one does not actually change the function itself! That is because the function relies on pointers in the background for its processes and those pointers will start to point to different places if the code is changed (in other words, the compiler will not compile the code the same way if one changes anything in the code...differntly compiled code means different machine code...which means that memory location will not be the same as before). As such, the provided example code will only work as long as nothing is changed in it. If one does wish to change things in the example, the machine code for the function that is being uploaded into the mcu within the push button IRQ will need to be adapted by hand (experience suggests that byte 0x800c01c and byte 0x800c036 are the pointers that will need to be checked).
