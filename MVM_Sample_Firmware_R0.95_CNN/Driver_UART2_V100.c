//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT)
//
//  (c) Copyright 2018, Fabian Kung Wai Lee, Selangor, MALAYSIA
//  All Rights Reserved  
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_UART_V100.c
// Author(s)		: Fabian Kung
// Last modified	: 28 June 2018
// Toolsuites		: Atmel Studio 7.0 or later
//					  GCC C-Compiler
//					  ARM CMSIS 5.4.0
//                    Atmel SAMS70_DFP 2.3.88		

#include "osmain.h"


// NOTE: Public function prototypes are declared in the corresponding *.h file.


//
// --- PUBLIC VARIABLES ---
//
// Data buffer and address pointers for wired serial communications (UART).

uint8_t gbytTXbuffer[__SCI_TXBUF_LENGTH-1];       // Transmit buffer.
uint8_t gbytTXbufptr;                             // Transmit buffer pointer.
uint8_t gbytTXbuflen;                             // Transmit buffer length.
uint8_t gbytRXbuffer[__SCI_RXBUF_LENGTH-1];       // Receive buffer length.
uint8_t gbytRXbufptr;                             // Receive buffer length pointer.

//
// --- PRIVATE VARIABLES ---
//


//
// --- Process Level Constants Definition --- 
//

//#define	_UART_BAUDRATE_kBPS	9.6	// Default datarate in kilobits-per-second, for HC-05 module.
//#define	_UART_BAUDRATE_kBPS 38.4	// Default datarate in kilobits-per-second for HC-05 module in AT mode.
#define	_UART_BAUDRATE_kBPS 115.2	// Default datarate in kilobits-per-second
//#define	_UART_BAUDRATE_kBPS 128.0	// Default datarate in kilobits-per-second
//#define	_UART_BAUDRATE_kBPS 230.4	// Default datarate in kilobits-per-second

///
/// Process name	: Proce_UART2_Driver
///
/// Author			: Fabian Kung
///
/// Last modified	: 7 June 2018
///
/// Code version	: 1.00
///
/// Processor		: ARM Cortex-M7 family                   
///
/// Processor/System Resource 
/// PINS		: 1. Pin PD25 = URXD2, peripheral C, input.
///  			  2. Pin PD26 = UTXD0=2, peripheral C, output.
///               3. PIN_ILED2 = indicator LED2.
///
/// MODULES		: 1. UART2 (Internal).
///               2. XDMAC (DMA Controller) Channel 1 (Internal).
///
/// RTOS		: Ver 1 or above, round-robin scheduling.
///
/// Global variable	: gbytRXbuffer[]
///                   gbytRXbufptr
///                   gbytTXbuffer[]
///                   gbytTXbufptr
///                   gbytTXbuflen
///                   gSCIstatus
///

#ifdef 				  __OS_VER		// Check RTOS version compatibility.
	#if 			  __OS_VER < 1
		#error "Proce_UART2_Driver: Incompatible OS version"
	#endif
#else
	#error "Proce_UART2_Driver: An RTOS is required with this function"
#endif

///
/// Description		: 1. Driver for built-in UART2 Module.
///                   2. Serial Communication Interface (UART) transmit buffer manager.
///                      Data will be taken from the SCI transmit buffer gbytTXbuffer in FIFO basis
///                      and transmitted via UART module.  Maximum data length is determined by the
///						 constant _SCI_TXBUF_LENGTH in file "osmain.h".
///                      Data transmission can be done with or without the assistance of the 
///                      DMA Controller (XDMAC).
///                   3. Serial Communication Interface (UART) receive buffer manager.
///					Data received from the USART module of the micro-controller will be
///					transferred from the USART registers to the SRAM of the micro-controller
///					called SCI receive buffer (gbytRXbuffer[]).
///					The flag bRXRDY will be set to indicate to the user modules that valid
///					data is present.
///					Maximum data length is determined by the constant _SCI_RXBUF_LENGTH in
///					file "osmain.h".
///
///
/// Example of usage : The codes example below illustrates how to send 2 bytes of character,
///			'a' and 'b' via UART without DMA assistance.
///          if (gSCIstatus.bTXRDY == 0)	// Check if any data to send via UART.
///          {
///             gbytTXbuffer[0] = 'a';	// Load data.
///		   	    gbytTXbuffer[1] = 'b';
///		   	    gbytTXbuflen = 2;		// Set TX frame length.
///		  	    gSCIstatus.bTXRDY = 1;	// Initiate TX.
///          }
///
/// Example of usage : The codes example below illustrates how to send 100 bytes of character,
///			 via UART with DMA assistance.  It is assumed the required data has been stored
///          in the transmit buffer gbytTXbuffer[0] to gbytTXbuffer[99] already.
///
/// 		SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
///                                 // (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
///                                 // from the cache, it may not contains the correct and up-to-date data.
///			XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
///			XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(100);	// Set number of bytes to transmit.
///			XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
///			gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
///			gSCIstatus.bTXRDY = 1;										// Initiate TX.
///			PIN_LED2_SET;												// Lights up indicator LED2.
///
/// Example of usage : The codes example below illustrates how to retrieve 1 byte of data from
///                    the UART receive buffer.
///			if (gSCIstatus.bRXRDY == 1)	// Check if UART receive any data.
///		    {
///             if (gSCIstatus.bRXOVF == 0) // Make sure no overflow error.
///			    {
///					bytData = gbytRXbuffer[0];	// Get 1 byte and ignore all others.
///             }
///             else
///             {
///					gSCIstatus.bRXOVF = 0; 	// Reset overflow error flag.
///             }
///             gSCIstatus.bRXRDY = 0;	// Reset valid data flag.
///             gbytRXbufptr = 0; 		// Reset pointer.
///			}
///
/// Note: Another way to check for received data is to monitor the received buffer pointer
/// gbytRXbufptr.  If no data this pointer is 0, a value greater than zero indicates the
/// number of bytes contain in the receive buffer.


void Proce_UART2_Driver(TASK_ATTRIBUTE *ptrTask)
{
	int		nTemp;
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - UART2 Initialization.
				// Setup IO pins mode and configure the peripheral pins:

				// Setup pin PD25 as input. General purpose input.
				PIOD->PIO_PPDDR |= PIO_PPDDR_P25;  // Disable internal pull-down to PD25.
				PIOD->PIO_PUER |= PIO_PUER_P25;	// Enable internal pull-up to PD25.
				PIOD->PIO_ODR |= PIO_ODR_P25;	// Disable output write to PD25, PD25 is used as input.
				//PIOD->PIO_IFER |= PIO_IFER_P25;	// Enable input glitch filter to PD25. This is optional.
 				
				 // 24 Nov 2015: To enable a peripheral, we need to:
 				// 1. Assign the IO pins to the peripheral.
 				// 2. Select the correct peripheral block (A, B, C or D).
 				PIOD->PIO_PDR = (PIOD->PIO_PDR) | PIO_PDR_P25;	// Set PD25 and PD26 to be controlled by Peripheral.
 				PIOD->PIO_PDR = (PIOD->PIO_PDR) | PIO_PDR_P26;	// UART2 resides in Peripheral block C, with 
																// PD25 = URXD2 and PD26 = UTXD2.
 																
 				PIOD->PIO_ABCDSR[0] = (PIOD->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P25;	// Select peripheral block C for
 				PIOD->PIO_ABCDSR[1] = (PIOD->PIO_ABCDSR[1]) | PIO_ABCDSR_P25;	// PD25.
 				PIOD->PIO_ABCDSR[0] = (PIOD->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P26;	// Select peripheral block C for
 				PIOD->PIO_ABCDSR[1] = (PIOD->PIO_ABCDSR[1]) | PIO_ABCDSR_P26;	// PD26.

				// Setup baud rate generator register.  
				// Baudrate = (Peripheral clock)/(16xCD)
				// for CD = 81, baud rate = 115.74 kbps
				// for CD = 977, baud rate = 9.596 kbps
				// for CD = 40, baud rate = 234.375 kbps
                if (_UART_BAUDRATE_kBPS == 230.4)
				{
					UART2->UART_BRGR = 41;						// Approximate 228.66 kbps
				}
				else
				{
					UART2->UART_BRGR = (__FPERIPHERAL_MHz*1000)/(16*_UART_BAUDRATE_kBPS);
				}
				                
				// Setup USART2 operation mode part 1:
				// 1. Enable UART2 RX and TX modules.
				// 2. Channel mode = Normal.
				// 3. 8 bits data, no parity, 1 stop bit.
				// 4. No interrupt.
				
				UART2->UART_MR = UART_MR_PAR_NO | UART_MR_CHMODE_NORMAL | UART_MR_BRSRCCK_PERIPH_CLK;	
																				// Set parity and channel mode, with baud
																				// rate generator (BRG) driven by peripheral
																				// clock.
				UART2->UART_CR = UART2->UART_CR | UART_CR_TXEN | UART_CR_RXEN; // Enable both transmitter and receiver.
				                                
				gbytTXbuflen = 0;               // Initialize all relevant variables and flags.
				gbytTXbufptr = 0;
				gSCIstatus.bRXRDY = 0;	
				gSCIstatus.bTXRDY = 0;
				gSCIstatus.bRXOVF = 0;
                                
				gbytRXbufptr = 0;
                PIN_LED2_CLEAR;							// Off indicator LED2.
				PMC->PMC_PCER1 |= PMC_PCER1_PID44;		// Enable peripheral clock to UART2 (ID44)
				OSSetTaskContext(ptrTask, 1, 100);		// Next state = 1, timer = 100.
			break;
			
			case 1: // State 1 - Initialization of XDMAC Channel 1, map to UART2 TX holding buffer.
					// Setup XDMAC Channel 1 to handle transfer of data from SRAM to UART_THR.
					// Channel allocation: 1.
					// Source: SRAM.
					// Destination:  UART2 TX (XDMAC_CC.PERID = 24).
					// Transfer mode (TYPE): Single block with single microblock. (BLEN = 0)
					// Memory burst size (MBSIZE): 1
					// Chunk size (CSIZE): 1 chunks
					// Channel data width (DWIDTH): byte.
					// Source address mode (SAM): increment.
					// Destination address mode (DAM): fixed.
							
				nTemp = XDMAC->XDMAC_CHID[1].XDMAC_CIS;			// Clear channel 1 interrupt status register.  This is a read-only register, reading it will clear all
							// interrupt flags.
				XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
				XDMAC->XDMAC_CHID[1].XDMAC_CDA = (uint32_t) &(UART2->UART_THR);	// Set destination start address.

				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(1);	// Set the number of data chunks in a microblock, default.  User
																		// to modify later.
				XDMAC->XDMAC_CHID[1].XDMAC_CC = XDMAC_CC_TYPE_PER_TRAN|	// Peripheral synchronized mode.
				XDMAC_CC_CSIZE_CHK_1|
				XDMAC_CC_MBSIZE_SINGLE|
				XDMAC_CC_DSYNC_MEM2PER|
				XDMAC_CC_DWIDTH_BYTE|
				XDMAC_CC_SIF_AHB_IF0|		// Data is read through this AHB Master interface, connects to SRAM.
				XDMAC_CC_DIF_AHB_IF1|		// Data is write through this AHB Master interface, connects to peripheral bus.
				XDMAC_CC_SAM_INCREMENTED_AM|
				XDMAC_CC_DAM_FIXED_AM|
				XDMAC_CC_SWREQ_HWR_CONNECTED| // Hardware request line is connected to the peripheral request line.
				XDMAC_CC_PERID(24);			// UART2 TX.  See datasheet.
							
				XDMAC->XDMAC_CHID[1].XDMAC_CNDC = 0;		// Next descriptor control register.
				XDMAC->XDMAC_CHID[1].XDMAC_CBC = 0;			// Block control register.
				XDMAC->XDMAC_CHID[1].XDMAC_CDS_MSP = 0;		// Data stride memory set pattern register.
				XDMAC->XDMAC_CHID[1].XDMAC_CSUS = 0;		// Source microblock stride register.
				XDMAC->XDMAC_CHID[1].XDMAC_CDUS = 0;		// Destination microblock stride register.		
					
				OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
			break;
			
			case 2: // State 2 - Transmit and receive buffer manager.
				// Check for data to send via UART.
				// Note that the transmit buffer is only 2-level deep in ARM Cortex-M4 micro-controllers.
				if (gSCIstatus.bTXRDY == 1)                         // Check if valid data in SCI buffer.
				{
					if (gSCIstatus.bTXDMAEN == 0)					// Transmit without DMA.
					{
						while ((UART2->UART_SR & UART_SR_TXRDY) > 0)// Check if UART transmit holding buffer is not full.
						{
							PIN_LED2_SET;							// On indicator LED2.
							if (gbytTXbufptr < gbytTXbuflen)		// Make sure we haven't reach end of valid data to transmit. 
							{
								UART2->UART_THR = gbytTXbuffer[gbytTXbufptr];	// Load 1 byte data to UART transmit holding buffer.
								gbytTXbufptr++;                     // Pointer to next byte in TX buffer.
							}
							else                                    // End of data to transmit.
							{
								gbytTXbufptr = 0;                   // Reset TX buffer pointer.
								gbytTXbuflen = 0;                   // Reset TX buffer length.
								gSCIstatus.bTXRDY = 0;              // Reset transmit flag.
								PIN_LED2_CLEAR;                     // Off indicator LED2.
								break;
							}
						}
                    }
					else											// Transmit with DMA.
					{
						if ((XDMAC->XDMAC_GS & XDMAC_GS_ST1_Msk) == 0)	// Check if DMA UART transmit is completed.
						{
							gSCIstatus.bTXRDY = 0;					// Reset transmit flag.
							PIN_LED2_CLEAR;							// Off indicator LED2.
							break;							
						}
					}
				}


				// Check for data to receive via UART.
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
				
				OSSetTaskContext(ptrTask, 2, 1); // Next state = 2, timer = 1.
			break;

			default:
				OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}


