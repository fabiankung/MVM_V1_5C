

#include "osmain.h"
//#include "Driver_UART2_V100.h"
#include "Driver_USART0_V100.h"
#include "Driver_I2C1_V100.h"
#include "Driver_LCD_ILI9341_V100.h"
#include "./Driver_TCM8230_LCD.h"	// For QVGA image resolution, color pixel data, and QQVGA, grayscale pixel data.

#include "User_Task.h"

// SysTick exception service routine (ISR).
void SysTick_Handler(void)
{
	gnRunTask++;
	gunClockTick++;
	ClearWatchDog();	
}

int main(void)
{
	int ni = 0;
	
	SAMS70_Init();				// Custom initialization, see file "os_SAMS70_APIs.c".  This will overwrites the
								// initialization done in SystemInit();
	OSInit();                   // Custom initialization: Initialize the RTOS.
	gnTaskCount = 0; 			// Initialize task counter.
	
	// Initialize core OS processes.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], OSProce1);						// Start main blinking LED process.
	
	// Initialize library processes.
	//OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_UART2_Driver);			// Start UART driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_I2C1_Driver);			// Start I2C driver.		
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_Camera_LED_Driver);
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_USART0_Driver);			// Start USART0 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_LCD_ILI9341_Driver);		// Start the 320x240 TFT LCD driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_TCM8230LCD_Driver);		// Start TCM8230 camera driver for QVGA resolution with RGB565 pixel buffer.
	
	#ifdef		__QQVGA__
	OSCreateTask(&gstrcTaskContext[gnTaskCount], ImageProcessingAlgorithm3);		// Start sample image processing algorithm.
	#endif
	
	/* Replace with your application code */
	while (1)
	{
		// --- Run all processes sequentially ---
		if (gnRunTask > 0) 		// Only execute tasks/processes when gnRunTask is not 0.
		{
			//PIOD->PIO_ODSR |= PIO_ODSR_P22;			// Set PD22, this is used as a output strobe to indicate the timing of the RTOS.
			for (ni = 0; ni < gnTaskCount; ni++)		// Using for-loop produce more efficient
			// assembly codes.
			{
				if (gstrcTaskContext[ni].nTimer > 0)	// Only decrement timer if it is greater than zero.
				{
					--(gstrcTaskContext[ni].nTimer);	// Decrement timer for each process.
				}
			} // for (ni = 0; ni < gnTaskCount; ni++)
			for (ni = 0; ni < gnTaskCount; ni++)
			{
				// Only execute a process/task if it's timer = 0.
				if (gstrcTaskContext[ni].nTimer == 0)
				{
					// Execute user task by dereferencing the function pointer.
					(*((TASK_POINTER)gfptrTask[ni]))(&gstrcTaskContext[ni]);
				}
			}
			gnRunTask = 0;								// Reset Run Task flag.
			//PIOD->PIO_ODSR &= ~PIO_ODSR_P22;			// Clear PD22.	
		}// if (gnRunTask > 0)	
	} // while (1)		
}
