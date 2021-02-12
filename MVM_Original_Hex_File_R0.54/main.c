


#include "osmain.h"
#include "Driver_UART2_V100.h"
#include "Driver_USART0_V100.h"
#include "Driver_I2C1_V100.h"
#include "Driver_TCM8230.h"


#include "User_task_0_54.h"

int main(void)
{
	int ni = 0;
	
	SAMS70_Init();				// Custom initialization, see file "os_SAMS70_APIs.c".  This will overwrites the
								// initialization done in SystemInit();
	OSInit();                   // Custom initialization: Initialize the RTOS.
	gnTaskCount = 0; 			// Initialize task counter.
	
	// Initialize core OS processes.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], OSProce1);							// Start main blinking LED process.
	
	// Initialize driver processes.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_UART2_Driver);				// Start UART driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_I2C1_Driver);				// Start I2C driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_Camera_LED_Driver);			// Start camera LED driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_USART0_Driver);				// Start USART0 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_TCM8230_Driver);				// Start Toshiba TCM8230 camera driver.
	
	// Initialize user processes.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_MessageLoop_StreamImage);	// Start stream image data via UART port process.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_RunImageProcess);			// Run various image process algorithm.
	
	
	while (1)
	{	
		// --- Check SysTick until time is up, then update each process's timer ---
		if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) > 0)		// Check if SysTick counts to 0 since the last read.
		{
			//PIOD->PIO_ODSR |= PIO_ODSR_P21;		// Set PD21, this is used as a output strobe to indicate the timing of the RTOS.
			OSEnterCritical();						// Stop all processor interrupts.
			if (gnRunTask == 1)						// If task overflow occur trap the controller
			{										// indefinitely and turn on indicator LED1.
				while (1)
				{
					ClearWatchDog();				// Clear the Watch Dog Timer.
					PIN_OSPROCE1_SET; 				// Turn on indicator LED1.
				}
			}

			gnRunTask = 1;							// Assert gnRunTask.
			gunClockTick++; 						// Increment RTOS clock tick counter.
			for (ni = 0; ni < gnTaskCount; ni++)	// Using for-loop produce more efficient
													// assembly codes.
			{
				if (gstrcTaskContext[ni].nTimer > 0) // Only decrement timer if it is greater than zero.
				{
					--(gstrcTaskContext[ni].nTimer); // Decrement timer for each process.
				}
			}

			OSExitCritical();						// Enable processor interrupts.
			ClearWatchDog();						// Clear the Watch Dog Timer regularly (at least once a second).
													// to prevent the MCU from self-reset.
			
			//PIOD->PIO_ODSR &= ~PIO_ODSR_P21;		// Clear PD21.  This pin serves as process strobe signal to check the
													// RTOS timing.
		}  // if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) > 0)

		// --- Run all processes sequentially ---
		if (gnRunTask > 0) 		// Only execute tasks/processes when gnRunTask is not 0.
		{
			for (ni = 0; ni < gnTaskCount; ni++)
			{
				// Only execute a process/task if it's timer = 0.
				if (gstrcTaskContext[ni].nTimer == 0)
				{
					// Execute user task by dereferencing the function pointer.
					(*((TASK_POINTER)gfptrTask[ni]))(&gstrcTaskContext[ni]);
				}
			}
			gnRunTask = 0; 		// Reset gnRunTask.
		} // if (gnRunTask > 0)
	} // while (1)
}
