


#include "osmain.h"
#include "Driver_UART2_V100.h"									
#include "Driver_USART0_V100.h"
#include "Driver_ESP8266_V100.h"
#include "Driver_I2C1_V100.h"
#include "Driver_TCM8230.h"

#include "User_Task.h"


// SysTick interrupt service routine (ISR).
void SysTick_Handler(void)
{
	static int	nCompareState = 0;
	
	//PIOD->PIO_ODSR |= PIO_ODSR_P21;			// Set PD21, this is used as a output strobe to indicate the timing of the RTOS.
	gnRunTask++;
	gunClockTick++;
	ClearWatchDog();
				
	// Check for data to receive via UART2.
	// Note that the receive FIFO buffer is only 2-level deep in ARM Cortex-M4 micro-controllers.
	// Here we ignore Parity error.  If overflow or framing error is detected, we need to write a 1
	// to the bit RSTSTA to clear the error flags.  It is also advisable to reset the receiver.
				
	if (((UART2->UART_SR & UART_SR_FRAME) == 0) && ((UART2->UART_SR & UART_SR_OVRE) == 0))
	{															// Make sure no hardware overflow and
		// and framing error.
		while ((UART2->UART_SR & UART_SR_RXRDY) > 0)			// Check for availability of data received.
																// available at UART
		{
			PIN_LED2_SET;										// On indicator LED2.
			if (gbytRXbufptr < __SCI_RXBUF_LENGTH)				// check for data overflow.
			{													// Read a character from USART.
				gbytRXbuffer[gbytRXbufptr] = UART2->UART_RHR;	// Get received data byte.
				gbytRXbufptr++;									// Pointer to next byte in RX buffer.
				gSCIstatus.bRXRDY = 1;							// Set valid data flag.
			}
			else 												// data overflow.
			{
				gbytRXbufptr = 0;								// Reset buffer pointer.
				gSCIstatus.bRXOVF = 1;							// Set receive data overflow flag.
				break;
			}
						
		}
	}
	else														// Hard overflow or/and framing error.
	{
		UART2->UART_CR = UART2->UART_CR | UART_CR_RSTSTA;		// Clear overrun and framing error flags.
		//UART2->UART_CR = UART2->UART_CR | UART_CR_RSTRX;		// Reset the receiver.
		gbytRXbufptr = 0;										// Reset buffer pointer.
		gSCIstatus.bRXOVF = 1;									// Set receive data overflow flag.
	}	
	//PIOD->PIO_ODSR &= ~PIO_ODSR_P21;				// Clear PD21.
}


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
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_USART0_Driver);				// Start USART0 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_ESP8266_Driver);				// Start ESP8266 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_I2C1_Driver);				// Start I2C1 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_TCM8230_Driver);				// Start camera driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_MessageLoop_StreamImage);	// Start stream image.
    OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_UART2_Driver);				// Start UART2 driver, putting UART2 process
																					// after stream image process is more 
																					// efficient.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_Echo_UART2_RX);				// Start Monitor UART2 RX port process.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_IPA1);						// Start image processing process.
																							
	while (1)
	{			
		// --- Run all processes sequentially ---
		// Only execute tasks/processes when gnRunTask is not 0. 
		//if (gnRunTask > 0)						// For SysTick timeout of 166.67 usec, UART2 baud = 56.7 kbps or lower.
		//if (gnRunTask == 3) 						// For SysTick timeout of 55.56 usec, UART2 baud = 115.2 kbps.
		//if (gnRunTask == 4)							// For SysTick timeout of 41.67 usec, UART2 baud = 230.4 kbps.
		if (gnRunTask == 6)							// For SysTick timeout of 27.78 usec, UART2 baud = 345.6 kbps.
		//if (gnRunTask == 8)							// For SysTick timeout of 20.83 usec, UART2 baud = 460.8 kbps.
		{			
			//PIOD->PIO_ODSR |= PIO_ODSR_P22;		// Set PD22.
			for (ni = 0; ni < gnTaskCount; ni++)	// Using for-loop produce more efficient
													// assembly codes.
			{
				if (gstrcTaskContext[ni].nTimer > 0) // Only decrement timer if it is greater than zero.
				{
					--(gstrcTaskContext[ni].nTimer); // Decrement timer for each process.
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
			gnRunTask = 0; 					
			//PIOD->PIO_ODSR &= ~PIO_ODSR_P22;				// Clear PD22.	
			
		} // if (gnRunTask > 0)
	} // while (1)
}
