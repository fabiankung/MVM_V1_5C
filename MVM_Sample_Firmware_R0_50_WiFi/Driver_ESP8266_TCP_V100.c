 //////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT)
//
//  (c) Copyright 2018-2021, Fabian Kung Wai Lee, Selangor, MALAYSIA
//  All Rights Reserved  
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_UART_V100.c
// Author(s)		: Fabian Kung
// Last modified	: 10 Dec 2021
// Toolsuites		: Atmel Studio 7.0 or later
//					  GCC C-Compiler
//					  ARM CMSIS 5.4.0		

#include "osmain.h"
#include "Driver_UART2_V100.h"
#include "Driver_USART0_V100.h"
#include <string.h>
#include "Driver_TCM8230.h"

// NOTE: Public function prototypes are declared in the corresponding *.h file.


//
// --- PUBLIC VARIABLES ---
//
int		gnESP8266Flag = -1;										// ESP8266 activity flag = -1;

//
// --- PRIVATE VARIABLES ---
//


//
// --- Process Level Constants Definition --- 
//
#define PIN_FLAG3_SET			PIOA->PIO_ODSR |= PIO_ODSR_P7;					// Set flag 3.
#define PIN_FLAG3_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P7;					// Clear flag 3.
#define PIN_FLAG4_SET			PIOA->PIO_ODSR |= PIO_ODSR_P8;					// Set flag 4.
#define PIN_FLAG4_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P8;					// Clear flag 4.

// 
// --- Local function
// 
void SendATCommandESP8266(unsigned char *);


///
/// Process name	: Proce_ESP8266_Driver
///
/// Author			: Fabian Kung
///
/// Last modified	: 10 Dec 2021
///
/// Code version	: 0.64
///
/// Processor		: ARM Cortex-M7 family                   
///
/// Processor/System Resource 
/// PINS		: 1. Pin PD25 = URXD2, peripheral C, input.
///  			  2. Pin PD26 = UTXD0=2, peripheral C, output.
///               3. PIN_ILED2 = indicator LED2.
///
/// MODULES		: 1. UART2 (Internal).
///               2. USART0 (Internal).
///               3. XDMAC (DMA Controller) Channel 1 (Internal).
///
/// RTOS		: Ver 1 or above, round-robin scheduling.
///
/// Global variable	: gbytRXbuffer[]
///                   gbytRXbufptr
///                   gbytTXbuffer[]
///                   gbytTXbufptr
///                   gbytTXbuflen
///                   gSCIstatus
///                   gbytRXbuffer2[]
///                   gbytRXbufptr2
///                   gbytTXbuffer2[]
///                   gbytTXbufptr2
///                   gbytTXbuflen2
///                   gSCIstatus2
///

#ifdef 				  __OS_VER		// Check RTOS version compatibility.
	#if 			  __OS_VER < 1
		#error "Proce_ESP8266_Driver: Incompatible OS version"
	#endif
#else
	#error "Proce_ESP8266_Driver: An RTOS is required with this function"
#endif

///
/// Description		: Process to initialize connected ESP8266 module as a TCP server.
///

#define _PROCE_ESP8266_DRIVER_TIMEOUT_PERIOD	1000*__NUM_SYSTEMTICK_MSEC

void Proce_ESP8266_Driver(TASK_ATTRIBUTE *ptrTask)
{
	static	int nTimeoutCounter = 0;
	static	int nLineCounter;
	int nIndex;
		
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization.
				//PIN_LED2_CLEAR;
				gnESP8266Flag = -1;												// Indicate ESP8266 Module driver is not ready.
				nLineCounter = 0;												// Reset pixel line counter.
				OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 1, timer = 1000msec.
				//OSSetTaskContext(ptrTask, 101, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 101, timer = 1000msec.
				//OSSetTaskContext(ptrTask, 102, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 102, timer = 1000msec.
				//OSSetTaskContext(ptrTask, 103, 5000*__NUM_SYSTEMTICK_MSEC);		// Next state = 103, timer = 5000msec.
			break;
			
			case 1: // State 1 - Setup ESP8266 Module, check if ESP8266 is present.
				SendATCommandESP8266("AT");
				//OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 2, timer = 1000msec.
				OSSetTaskContext(ptrTask, 2, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 2, timer = 1000msec.
			break;
			
			case 2: // State 2 - Reset ESP8266 module.
				SendATCommandESP8266("AT+RST");
				OSSetTaskContext(ptrTask, 3, 3500*__NUM_SYSTEMTICK_MSEC);	// Next state = 3, timer = 3500msec.			
			break;
			
			case 3: // State 3 - Set to soft access point (AP) mode.
				SendATCommandESP8266("AT+CWMODE=2");
				OSSetTaskContext(ptrTask, 4, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 4, timer = 1000msec.
			break;
			
			case 4: // State 4 - Configure the access point.
			        // SSID = "ESP", password = "password", 1 channel,
			        // encryption = WPA_WPA2_PSK
				SendATCommandESP8266("AT+CWSAP_CUR=\"ESP1\",\"esp8266fkwl\",1,4"); // NOTE: Password must be at least 8 characters.
				OSSetTaskContext(ptrTask, 5, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 5, timer = 1000msec.
			break;
			
			case 5: // State 5 - Configure for multiple connections.
				SendATCommandESP8266("AT+CIPMUX=1");
				OSSetTaskContext(ptrTask, 6, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 6, timer = 1000msec.
			break;
					
			case 6: // State 6 - Turn on TCP server on port 222.
				SendATCommandESP8266("AT+CIPSERVER=1,222");
				OSSetTaskContext(ptrTask, 7, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 7, timer = 1000msec.
			break;
			
			case 7: // State 7 - Set TCP server time out period to 10 seconds.
				SendATCommandESP8266("AT+CIPSTO=10");
				OSSetTaskContext(ptrTask, 8, 1000*__NUM_SYSTEMTICK_MSEC);	// Next state = 8, timer = 1000msec.
			break;	
			
			case 8: // State 8 - Get default IP address of ESP8266 softAP.
			    // default IP: 192.168.4.1
				SendATCommandESP8266("AT+CIFSR");																	
				OSSetTaskContext(ptrTask, 9, 1250*__NUM_SYSTEMTICK_MSEC);	// Next state = 9, timer = 1250msec.
			break;		
			
			case 9: // State 9 - Switch off AT command echoing. Hopefully this will speed up the bilateral communication.
				SendATCommandESP8266("ATE0");
				OSSetTaskContext(ptrTask, 20, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 20, timer = 1000msec.
			break;
			
			case 20: // State 20 - Indicate ESP8266 module is ready.
				gnESP8266Flag = 0;												// Clear activity flag first.
				UART2->UART_CMPR = 0;											// Reset comparison control register 1st.
				UART2->UART_CMPR = UART_CMPR_CMPMODE_FLAG_ONLY | UART_CMPR_VAL1('X') // Enable comparison function in UART2.
								| UART_CMPR_VAL2('X');							// Compare with character 'X'.
				OSSetTaskContext(ptrTask, 21, 10*__NUM_SYSTEMTICK_MSEC);		// Next state = 21, timer = 10 msec .
			break;
			
			case 21: // State 21 - Check if receive 'X' from ESP8266.
				if ((UART2->UART_SR & UART_SR_CMP_Msk) > 0)						// Check if CMP flag is set, e.g. 'X' is received by UART2.
				{
					//PIOA->PIO_ODSR |= PIO_ODSR_P8;								// Set PA8.
					UART2->UART_CR = UART2->UART_CR | UART_CR_RSTSTA;			// Clear CMP flag.
					gnESP8266Flag = 1;	
					UART2->UART_CMPR = 0;	// Reset comparison control register.
					UART2->UART_CMPR = UART_CMPR_CMPMODE_FLAG_ONLY | UART_CMPR_VAL1('>')	// Enable comparison function in UART2.
									   | UART_CMPR_VAL2('>');								// Compare with character '>'.	
					OSSetTaskContext(ptrTask, 22, 1);							// Next state = 22, timer = 1.				
				}	
				else
				{
					OSSetTaskContext(ptrTask, 21, 1);							// Next state = 21, timer = 1.			
				}
			break;
			
			case 22: // State 22 - Check if receive '>' from ESP8266.
				if ((UART2->UART_SR & UART_SR_CMP_Msk) > 0)						// Check if CMP flag is set, e.g. '>' is received by UART2.
				{
					//PIOA->PIO_ODSR |= PIO_ODSR_P8;								// Set PA8.
					UART2->UART_CR = UART2->UART_CR | UART_CR_RSTSTA;			// Clear CMP flag.
					gnESP8266Flag = 2;
					UART2->UART_CMPR = 0;											// Reset comparison control register 1st.
					UART2->UART_CMPR = UART_CMPR_CMPMODE_FLAG_ONLY | UART_CMPR_VAL1('X') // Enable comparison function in UART2.	
					| UART_CMPR_VAL2('X');										// Compare with character 'X'.				
					OSSetTaskContext(ptrTask, 21, 1);							// Next state = 21, timer = 1.
				}
				else
				{
					OSSetTaskContext(ptrTask, 22, 1);							// Next state = 22, timer = 1.
				}			
			break;
					
			case 100: // State 100 - Idle.
				OSSetTaskContext(ptrTask, 100, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 100, timer = 1000msec.
			break;
			
			case 101: // State 101 - Test USART0, no DMA.
				gbytTXbuffer2[0] = 'A';
				gbytTXbuffer2[1] = 'B';	
				gbytTXbuflen2 = 2;			// Set USART0 TX frame length.
				gSCIstatus2.bTXRDY = 1;		// Initiate TX.
				OSSetTaskContext(ptrTask, 101, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 101, timer = 1000msec.
			break;
			
			case 102: // State 102 - Test USART0 using DMA.
				if (gSCIstatus2.bTXRDY == 0)
				{
					SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
					// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
					// from the cache, it may not contains the correct and up-to-date data.
					gbytTXbuffer2[0] = 'H';
					gbytTXbuffer2[1] = 'e';
					gbytTXbuffer2[2] = 'l';
					gbytTXbuffer2[3] = 'l';
					gbytTXbuffer2[4] = 'o';
					gbytTXbuffer2[5] = '\n';
					XDMAC->XDMAC_CHID[2].XDMAC_CSA = (uint32_t) gbytTXbuffer2;	// Set source start address.
					XDMAC->XDMAC_CHID[2].XDMAC_CUBC = XDMAC_CUBC_UBLEN(6);		// Set number of bytes to transmit.
					XDMAC->XDMAC_GE = XDMAC_GE_EN2;								// Enable channel 2 of XDMAC.
					gSCIstatus2.bTXDMAEN = 1;									// Indicate USART0 transmit with DMA.
					gSCIstatus2.bTXRDY = 1;										// Initiate TX.
					PIN_LED2_SET;												// Lights up indicator LED2.
				}
				OSSetTaskContext(ptrTask, 102, 100*__NUM_SYSTEMTICK_MSEC);		// Next state = 102, timer = 100msec.	
			break;
			
			case 103: // State 103 - Change the default baud rate in ESP8266 module.
				SendATCommandESP8266("AT+UART_DEF=230400,8,1,0,0");
				OSSetTaskContext(ptrTask, 100, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 100, timer = 100msec.	
			break;
			
			default:
				OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}

// Function to generate all the necessary instructions to send a string via UART2.
void SendATCommandESP8266(unsigned char *strCommand)
{
	int		nCommLen;
				
	nCommLen = strlen(strCommand);		// Calculate command length in number of bytes.
	strcpy(gbytTXbuffer,strCommand);	// Load command into transmit buffer.
	gbytTXbuffer[nCommLen] = 0xD;		// Append carriage Return or CR to transmit data.  Note: always end commend with NL and CR.
	gbytTXbuffer[nCommLen+1] = 0xA;		// NL or '\n'
	SCB_CleanDCache();					// If we are using data cache (D-Cache), we should clean the data cache
										// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
										// from the cache, it may not contains the up-to-date and correct data.
	XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
	XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nCommLen+2);		// Set number of bytes to transmit.
	XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
	//SCB_InvalidateDCache();										// Mark the data cache as invalid. Subsequent read from DCache forces data to be copied from SRAM to
																// the cache.  This is to be used after XDMAC updates the SRAM without the knowledge of the CPU's cache
																// controller.
	gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
	gSCIstatus.bTXRDY = 1;										// Initiate TX.
	PIN_LED2_SET;												// Lights up indicator LED2.	
}
