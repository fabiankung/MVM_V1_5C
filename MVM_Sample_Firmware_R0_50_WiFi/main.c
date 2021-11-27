


#include "osmain.h"
#include "Driver_UART2_V100.h"									
#include "Driver_USART0_V100.h"
#include "Driver_ESP8266_V100.h"
#include "Driver_I2C1_V100.h"
#include "Driver_TCM8230.h"

#include "User_Task.h"

//#define		__DEBUG_UART2__				// Uncomment this statement during debugging. This will allow USART0 to echo all the bytes 
											// received by UART2. If UART2 is connected to Bluetooth or WiFi module, then we can see
											// the data send by the module.

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
	
	//PIOA->PIO_ODSR &= ~PIO_ODSR_P8;								// Clear PA8.
	
	// Transmit buffer manager for USART0.	
	// Check for data to send via USART0.
	// Note that the transmit buffer is only 2-level deep in ARM Cortex-M4 micro-controllers.
	if (gSCIstatus2.bTXRDY == 1)					// Check if valid data in SCI buffer.
	{	
		while ((USART0->US_CSR & US_CSR_TXRDY) > 0)	// Check if USART transmit holding buffer is not full.
		{
			PIN_LED2_SET;							// On indicator LED2.
			if (gbytTXbufptr2 < gbytTXbuflen2)		// Make sure we haven't reach end of valid data to transmit.
			{
				USART0->US_THR = gbytTXbuffer2[gbytTXbufptr2];	// Load 1 byte data to USART transmit holding buffer.
				gbytTXbufptr2++;                    // Pointer to next byte in TX buffer.
			}
			else                                    // End of data to transmit.
			{
				gbytTXbufptr2 = 0;                  // Reset TX buffer pointer.
				gbytTXbuflen2 = 0;                  // Reset TX buffer length.
				gSCIstatus2.bTXRDY = 0;             // Reset transmit flag.
				PIN_LED2_CLEAR;                     // Off indicator LED2.
				break;
			}
		}
	}				
		
	// Receive buffer manager for UART2.
	int     ni;
	unsigned int	unData;
	if (gSCIstatus.bRXRDY == 1)					// Check if UART2 receive any data.
	{
		if (gSCIstatus.bRXOVF == 0)				// Make sure no overflow error.
		{
			if (gSCIstatus2.bTXRDY == 0)		// Make sure USART0 is idle.
			{

#ifdef __DEBUG_UART2__	// If debugging mode is enabled run the following codes, echo UART2 received buffer to USART0 transmit.
				
				for (ni = 0; ni < gbytRXbufptr; ni++)	// Go through all bytes in the UART2 receive buffer.
				{
					unData = gbytRXbuffer[ni];
					gbytTXbuffer2[ni] = unData;	// Load data from UART2 receive
												// buffer to USART0 transmit buffer.	
				}
				// --- Enable this if we wish to echo the data received from host to USART0 ---
				gbytTXbuflen2 = ni;				// Set USART0 TX frame length.
				gSCIstatus2.bTXRDY = 1;			// Initiate TX.
				// ----------------------------------------------------------------------------
#endif 				
				gSCIstatus.bRXRDY = 0;			// Reset valid data flag.
				gbytRXbufptr = 0; 				// Reset pointer.
				PIN_LED2_CLEAR;					// Off indicator LED2.
			}
		}
		else
		{
			gSCIstatus.bRXOVF = 0; 				// Reset overflow error flag.
			gSCIstatus.bRXRDY = 0;				// Reset valid data flag.
			gbytRXbufptr = 0; 					// Reset pointer.
			PIN_LED2_CLEAR;						// Off indicator LED2.
		}
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
	//OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_UART2_Driver);				// Start UART2 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_USART0_Driver);				// Start USART0 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_ESP8266_Driver);				// Start ESP8266 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_I2C1_Driver);				// Start I2C1 driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_TCM8230_Driver);				// Start camera driver.
	OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_MessageLoop_StreamImage);	// Start stream image.
    OSCreateTask(&gstrcTaskContext[gnTaskCount], Proce_UART2_Driver);				// Start UART2 driver, putting UART2 process
																					// after stream image process is more 
																					// efficient.		
	 while (1)
	{			
		// --- Run all processes sequentially ---
		// Only execute tasks/processes when gnRunTask is not 0. 
		//if (gnRunTask > 0)						// For SysTick timeout of 166.67 usec, UART2 baud = 56.7 kbps or lower.
		//if (gnRunTask == 3) 						// For SysTick timeout of 55.56 usec, UART2 baud = 115.2 kbps.
		//if (gnRunTask == 4)							// For SysTick timeout of 41.67 usec, UART2 baud = 230.4 kbps.
		if (gnRunTask == 6)							// For SysTick timeout of 27.78 usec, UART2 baud = 345.6 kbps.
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
