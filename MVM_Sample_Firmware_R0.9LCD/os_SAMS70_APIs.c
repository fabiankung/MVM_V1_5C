///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  APPLICATION PROGRAM INTERFACE ROUTINES FOR SAMS70 SERIES ARM CORTEX-M7 MICROCONTROLLER
//
//  (c) Copyright 2015-2018, Fabian Kung Wai Lee, Selangor, MALAYSIA
//  All Rights Reserved  
//   
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Filename			: os_SAMS70_APIs.c
// Author			: Fabian Kung
// Last modified	: 15 October 2018
// Version			: 1.01
// Description		: This file contains the implementation of all the important routines
//                    used by the OS and the user routines. Most of the routines deal with
//                    micro-controller specifics resources, thus the functions have to be
//                    rewritten for different micro-controller family. Only the function
//                    prototype (call convention) of the routine is to be maintained.  In this
//                    way the OS can be ported to different micro-controller family.
// Toolsuites		: AtmelStudio 7.0 or above
//                	  GCC C-Compiler 
//					  ARM CMSIS 5.4.0
//                    SAMS70_DFP 2.3.88
// Micro-controller	: Atmel SAMS70J19 or SAMS70J20 ARM Cortex-M7 families.

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one changes folder
#include "osmain.h"

// --- GLOBAL AND EXTERNAL VARIABLES DECLARATION ---


// --- FUNCTIONS' PROTOTYPES ---


// --- FUNCTIONS' BODY ---

//////////////////////////////////////////////////////////////////////////////////////////////
//  BEGINNING OF CODES SPECIFIC TO ATSAM4S70 MICROCONTROLLER	//////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////


// Function name	: ClearWatchDog 
// Author			: Fabian Kung
// Last modified	: 26 May 2018
// Purpose			: Reset the Watch Dog Timer
// Arguments		: None
// Return			: None
// Description		: On the ARM Cortex-M micro-controller, the Watchdog Timer is enabled by default
//                    after power on reset.  The timeout period is determined by the 12-bits field 
//                    WDV of the WDT_MR register. The default value is 0xFFF.  The Watchdog Timer is
//                    driven by internal slow clock of 32.768 kHz with pre-scaler of 128, which work
//                    out to 16 seconds timeout period.
inline void ClearWatchDog(void)
{
	WDT->WDT_CR =  WDT_CR_WDRSTT | WDT_CR_KEY_PASSWD;	// Reload the Watchdog Timer.
														// Note that to prevent accidental
														// write, this register requires a Key or password.
}

/// Function Name	: SAMS70_Init
/// Author			: Fabian Kung
/// Last modified	: 30 Sep 2020
/// Description		: This function performs further initialization of the Cortex-M
///                   processor, namely:
///                   1. Disable ERASE function on pin PB12, so that we can use this pin as normal I/O pin.
///					  2. Setup processor main oscillator and clock generator circuit.
///                   3. Setup processor flash memory controller wait states.
///                   4. Setup the SysTick system timer and exception request.
///                   5. Enable the cache controller.
///					  6. Also initialized the micro-controller peripherals and
///                      I/O ports to a known state. For I/O ports, all pins will
///                      be set to
///                     (a) Assign to PIO module (PIOA to PIOC)
///					    (b) Digital mode,
///                     (c) Output and
///                     (d) A logic '0'.
/// Arguments		: None
/// Return			: None
/// 
/// Comments about latest version. Since this is the first routine the processor will run on power up, I added a delay the
/// routine. This is from observation that if we start to setup the processor immediately upon power up, sometimes the 
/// processor cannot boot up properly due to fluctuation in power supply voltage.  So perhaps it is good to delay for a
/// short while before setting up of the processor peripheral commence.
/// 
void SAMS70_Init()
{	
	int ni;
	
	for (ni = 0; ni < 150000; ni++)	// A short delay for the system to stabilize. Assuming initially the chip power up with
	{}									// 12MHz RC oscillator and 1 instruction per cycle, this would corresponds to
										// about 125.0 msec.
	
	// Since pin PB12 is also used as erase control pin for the on-chip flash memory and NVM (non-volatile memory) bits,
	// we can disable this function so that PB12 can be used as a normal I/O pin, this is crucial for 64 pins device.
	// Note: 4 July 2018.  If PB12 is used as a normal I/O pin and connected to other components, it is important that this
	// pin is held low during start-up.  Internally the ERASE pin integrates a pull-down resistor of about 100 kOhm to GND.
	// However it is still susceptible to any power glitch during start-up, especially if PB12 is connected to external
	// devices.  So it is best to disable the ERASE function as early as possible.
	
	MATRIX->CCFG_SYSIO = (MATRIX->CCFG_SYSIO) | CCFG_SYSIO_SYSIO12;	// PB12 function selected, ERASE function disabled.

	EFC->EEFC_FMR = EEFC_FMR_FWS(6);	// Set the wait states for Enhanced Flash Controller. FWS=6 allows for maximum KCLK of 300 MHz
	// and MCK of 150 MHz.

	// Setup Port A, Port B and Port D IO ports as output by default, except those pins which will be used
	// by internal peripheral as inputs.
	
									// PA0-PA2, PA6-PA8, PA15-PA20, PA23-PA31 are outputs, the rest inputs.
	PIOA->PIO_PER = 0xFFFFFFFF;		// PA1-PA32 are controlled by PIOA.									
	PIOA->PIO_OER = 0xFF9F81C7;		// Enable the output buffers of PA0-PA2, PA6-PA8, PA15-PA20, PA23-PA31.
	PIOA->PIO_OWER = 0xFF9F81C7;	// Enable the write operation to PIO_ODSR register of PA0-PA2, PA6-PA8, PA15-PA20, PA23-PA31. 
									// (which allow us to set or clear
									// each buffer using the ODSR register).
									
									// PB0 is RX of UART2, so it should remain as input.
	PIOB->PIO_PER = 0xFFFFFFFE;		// PB1-PB31 are controlled by PIOB.
	PIOB->PIO_OER = 0xFFFFFFFE;		// Enable the output buffers of PB1-PB31.
	PIOB->PIO_OWER = 0xFFFFFFFE;	// Enable the write operation to PIO_ODSR register.
									// (which allow us to set or clear
									// each buffer using the ODSR register).

									// PD25 is RX of USART0, so it should remain as input.
	PIOD->PIO_PER = 0xFDFFFFFF;		// PD0-PD31, except PD25 are controlled by PIOD.
	PIOD->PIO_OER = 0xFDFFFFFF;		// Enable output buffer of PD0-PD31, except PD25.
	PIOD->PIO_OWER = 0xFDFFFFFF;	// Enable the write operation to PIO_ODSR register, except PD25.
	
	PMC->PMC_PCER0 |= PMC_PCER0_PID16;	// Enable peripheral clock to PIOD.
	PMC->PMC_PCR = PMC_PCR_EN | PMC_PCR_CMD | PMC_PCR_PID(10);		// Enable peripheral clock to PIOA.
	PMC->PMC_PCR = PMC_PCR_EN | PMC_PCR_CMD | PMC_PCR_PID(11);		// Enable peripheral clock to PIOB.

	// --- Setting up oscillator and clock ---
	// NOTE: From the datasheet of SAM S70.
	// On power up reset:
	// 1) The default oscillator enabled is the internal 4 MHz RC in the MCU.
	// 2) The Main Clock (MAINCK) is derived from the 4 MHz RC oscillator.
	// 3) MAINCK is used to drive the master clock MCK and processor clock HCLK.
	// We need to change the default oscillator to the external crystal oscillator, enable
	// phase-locked loop (PLL) A, and use the PLLA to drive the MCK and HCLK.
	// Input to the PLLA should be between 8-30 MHz, and output of PLLA should be
	// between 160-500 MHz.
	
	/* Initialize the 8MHz main crystal oscillator */
	// This is done by setting the bit CKGR_MOR_MOSCXTEN.  Since we still need the RC oscillator to provide the timing,
	// the flag CKGR_MOR_MOSCRCEN should also be kept to '1'.
	// Wait for 100x8 = 800 slow clock cycles for the crystal oscillator to stabilize.
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD | CKGR_MOR_MOSCXTST(100) | CKGR_MOR_MOSCRCEN | CKGR_MOR_MOSCXTEN;

	while ( !(PMC->PMC_SR & PMC_SR_MOSCXTS) )	// Wait until the main crystal oscillator is stable.
	{
	}

	/* Switch to the 12MHz crystal oscillator */
	PMC->CKGR_MOR = CKGR_MOR_KEY_PASSWD | CKGR_MOR_MOSCXTST(100) | CKGR_MOR_MOSCRCEN | CKGR_MOR_MOSCXTEN | CKGR_MOR_MOSCSEL;
	while ( !(PMC->PMC_SR & PMC_SR_MOSCSELS) )	// Wait until the main oscillator selection is complete.
	{
	}

	//PMC->PMC_MCKR = (PMC->PMC_MCKR & ~(uint32_t)PMC_MCKR_CSS_Msk) | PMC_MCKR_CSS_MAIN_CLK;
	// Switch to main clock MAINCK.  Actually no need
	// to execute this as main clock is selected
	// by default on power up reset.
	//while ( !(PMC->PMC_SR & PMC_SR_MCKRDY) )
	//{
	//}
	
	/* Initialize and enable PLLA */
	// PLLA output frequency = [(Input Frequency)/DIVA]*(MULA + 1)
	// Note that input frequency to PLLA should be between 8-32 MHz.  In order to obtain 300 MHz output,
	// it is recommended to use 12 MHz crystal oscillator, set fin = 12 MHz, DIVA = 1, and MULA = 25.
	PMC->CKGR_PLLAR = CKGR_PLLAR_DIVA(1) | CKGR_PLLAR_PLLACOUNT(100) | CKGR_PLLAR_ONE | CKGR_PLLAR_MULA(25);	// PLLA output = 25x12 = 300 MHz
	while ( !(PMC->PMC_SR & PMC_SR_LOCKA) )		// Wait until PLLA is phase-locked.
	{
	}

	/* Switch clock source to PLLA */
	// PMC_MCKR must not be programmed in a single write operation, follow the sequence as in the datasheet.
	// Processor clock HCLK = PLLA Frequency.
	// Master clock MCK (to AHB bus and peripherals) = PLLA Frequency
	// Note: MCK needs to be less than 150 MHz for SAMS70/E70/V70
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_PRES_Msk) | PMC_MCKR_PRES_CLK_1;		// Set main clock pre-scaler first.
	while ( !(PMC->PMC_SR & PMC_SR_MCKRDY) )		// Wait for the clock switching to complete.
	{
	}
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_MDIV_Msk) | PMC_MCKR_MDIV_PCK_DIV2;	// Set main clock divider.
	//PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_MDIV_Msk) | PMC_MCKR_MDIV_EQ_PCK;	// Set main clock divider.
	while ( !(PMC->PMC_SR & PMC_SR_MCKRDY) )		// Wait for the clock switching to complete.
	{
	}
	PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) | PMC_MCKR_CSS_PLLA_CLK;	// Finally select PLLA as clock source.
	while ( !(PMC->PMC_SR & PMC_SR_MCKRDY) )		// Wait for the clock switching to complete.
	{
	}

	// Disable all clock signals to non-critical peripherals as default (to save power).
	// Note: Peripherals 0-6 are system critical peripheral such as Supply Controller, Reset Controller, Real-Time Clock/Timer, Watchdog Timer,
	// Power Management Controller and Flash Controller.  The clock to these peripherals cannot be disabled.

	

													
	//SCB_DisableDCache();			// Turn off data cache (D-Cache).  Data cache is turn on by default on reset.  No issue
									// with using D-cache if the data transfer is only between internal SRAM and Cortex-M7.
									// But it could be a problem if we want to transfer data between SRAM and peripherals
									// via AHB Matrix.  For instance when using XDMAC.
									// Alternatively we can activate D-cache, and clean the D-cache before a peripheral 
									// is accessing SRAM.  
									
	PMC->PMC_PCER1 |= PMC_PCER1_PID58;	// Enable peripheral clock to XDMAC.
					
	// Note: 24 Aug 2020, if SysTick_CTRL_CLKSOURCE = 0, SysTick is being triggered with external clock.
	// For ATSAMS70 family this means fCore divide by 2, see SAMS70 datasheet (2019 version) chapter 31.
	// Thus for fCore = 300 MHz, this works out to 150 MHz, and a count of 150000 will have a timeout period of 1 msec.
	// If we set SysTick_CTRL_CLKSOURCE = 1, then SysTick will be driven by peripheral clock, which is limited to
	// 150 MHz for SAMS70 family.  Thus for SAMS70, it does not matter whether we set or clear SysTick_CTRL_CLKSOURCE
	// bit.
	//SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;	// Clear this flag, clock source for SysTick from external clock.
	//SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;	// Set this flag, clock source for SysTick from peripheral
	// clock.
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;	// Enable SysTick exception request when count down to zero.
	SysTick->LOAD = __SYSTICKCOUNT;	// Set reload value.
	SysTick->VAL = __SYSTICKCOUNT;	// Reset current SysTick value.
	SysTick->CTRL = SysTick->CTRL & ~(SysTick_CTRL_COUNTFLAG_Msk);	// Clear Count Flag.
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;	// Enable SysTick.
	
	// Note: 17 Sep 2018: 
	// 1. Data and Instruction L1 cache are enable by setting the flags in CCR (Configuration and Control Register)
	// in the SCB (System Control Block region of the Cortex M7 memory).
	// 2. The cache policy for the L1 Cache can be changed by setting the flags in CACR (Cache Control Register) in the SCB.  
	// Note that CACR register is optional and may not be available in all Cortex-M devices.
	// Enable the Cortex-M7 Cache Controller for instruction and data caches.
	SCB_EnableICache();		// Invalidate then re-enable the instruction cache.
	SCB_EnableDCache();		// Invalidate then re-enable the data cache.
}

// Function name	: OSEnterCritical
// Author			: Fabian Kung
// Last modified	: 24 April 2007
// Description		: Disable all processor interrupts for important tasks
//					  involving Stacks, Program Counter other critical
//	                  processor registers.
void OSEnterCritical(void)
{

}											

// Function name	: OSExitCritical
// Author		: Fabian Kung
// Last modified	: 24 April 2007
// Description		: Enable all processor interrupts for important tasks.
void OSExitCritical(void)
{
 
}											


// Function name	: OSProce1
// Author			: Fabian Kung
// Last modified	: 26 May 2018
// Description		: Blink an indicator LED1 to show that the micro-controller is 'alive'.
#define _LED1_ON_US	500000		// LED1 on period in usec, i.e. 500msec.

void OSProce1(TASK_ATTRIBUTE *ptrTask)
{
    switch (ptrTask->nState)
    {
	case 0: // State 0 - On Indicator LED1
            PIN_OSPROCE1_SET;													// Turn on LED.                                      
            OSSetTaskContext(ptrTask, 1, _LED1_ON_US/__SYSTEMTICK_US);          // Next state = 1, timer = _LED_ON_US.
	break;

	case 1: // State 1 - Off Indicator LED1
            PIN_OSPROCE1_CLEAR;													// Turn off LED.                    
            OSSetTaskContext(ptrTask, 0, _LED1_ON_US/__SYSTEMTICK_US);          // Next state = 0, timer = 5000.
            break;

        default:
            OSSetTaskContext(ptrTask, 0, 0);                                    // Back to state = 0, timer = 0.
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////
//  END OF CODES SPECIFIC TO SAMS70 MICROCONTROLLER	   ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
