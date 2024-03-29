//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT)
//
//  (c) Copyright 2018-2021, Fabian Kung Wai Lee, Selangor, MALAYSIA
//  All Rights Reserved  
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_USART0_V100.c
// Author(s)		: Fabian Kung
// Last modified	: 18 Dec 2021
// Toolsuites		: Atmel Studio 7.0 or later
//					  GCC C-Compiler
//					  ARM CMSIS 5.4.0	

#include "osmain.h"
#include "Driver_USART0_V100.h"

// NOTE: Public function prototypes are declared in the corresponding *.h file.


//
// --- PUBLIC VARIABLES ---
//
// Data buffer and address pointers for wired serial communications (UART).
uint8_t gbytTXbuffer2[__SCI_TXBUF2_LENGTH-1];       // Transmit buffer.
uint8_t gbytTXbufptr2;                             // Transmit buffer pointer.
uint8_t gbytTXbuflen2;                             // Transmit buffer length.
uint8_t gbytRXbuffer2[__SCI_RXBUF2_LENGTH-1];       // Receive buffer length.
uint8_t gbytRXbufptr2;                             // Receive buffer length pointer.

SCI_STATUS gSCIstatus2;

//
// --- PRIVATE VARIABLES ---
//


//
// --- Process Level Constants Definition --- 
//

//#define	_USART_BAUDRATE_kBPS 19.2	// Default datarate in kilobits-per-second
//#define	_USART_BAUDRATE_kBPS 38.4	// Default datarate in kilobits-per-second
//#define	_USART_BAUDRATE_kBPS 57.6	// Default datarate in kilobits-per-second
//#define	_USART_BAUDRATE_kBPS 115.2	// Default datarate in kilobits-per-second
#define	_USART_BAUDRATE_kBPS 230.4	// Default datarate in kilobits-per-second
//#define	_USART_BAUDRATE_kBPS 500.0	// Default datarate in kilobits-per-second


///
/// Process name	: Proce_USART0_Driver
///
/// Author			: Fabian Kung
///
/// Last modified	: 8 Dec 2021
///
/// Code version	: 1.10
///
/// Processor		: ARM Cortex-M4 family                   
///
/// Processor/System Resource 
/// PINS		: 1. Pin PB0 = RXD0, peripheral C, input.
///  			  2. Pin PB1 = TXD0, peripheral C, output.
///               3. PIN_ILED2 = indicator LED2.
///
/// MODULES		: 1. USART0 (Internal).
///
/// RTOS		: Ver 1 or above, round-robin scheduling.
///
/// Global variable	: gbytRXbuffer2[]
///                   gbytRXbufptr2
///                   gbytTXbuffer2[]
///                   gbytTXbufptr2
///                   gbytTXbuflen2
///                   gSCIstatus2
///

#ifdef 				  __OS_VER		// Check RTOS version compatibility.
	#if 			  __OS_VER < 1
		#error "Proce_USART0_Driver: Incompatible OS version"
	#endif
#else
	#error "Proce_USART0_Driver: An RTOS is required with this function"
#endif

///
/// Description		: 1. Driver for built-in USART0 Module.
///                   2. Serial Communication Interface (UART) transmit buffer manager.
///                      Data will be taken from the SCI transmit buffer gbytTX2buffer in FIFO basis
///                      and transmitted via USART module.  Maximum data length is determined by the
///						 constant _SCI_TXBUF2_LENGTH in file "osmain.h".
///                      Data transmission can be done with or without the assistance of the Peripheral
///                      DMA Controller (PDC).
///                   3. Serial Communication Interface (UART) receive buffer manager.
///					Data received from the USART module of the micro-controller will be
///					transferred from the USART registers to the RAM of the micro-controller
///					called SCI receive buffer (gbytRX2buffer[]).
///					The flag bRXRDY will be set to indicate to the user modules that valid
///					data is present.
///					Maximum data length is determined by the constant _SCI_RXBUF2_LENGTH in
///					file "osmain.h".
///
///
/// Example of usage : The codes example below illustrates how to send 2 bytes of character,
///			'a' and 'b' via USART without PDC assistance.
///          if (gSCIstatus2.bTXRDY == 0)	// Check if any data to send via UART.
///          {
///             gbytTXbuffer2[0] = 'a';	// Load data.
///		   	    gbytTXbuffer2[1] = 'b';
///		   	    gbytTXbuflen2 = 2;		// Set TX frame length.
///		  	    gSCIstatus2.bTXRDY = 1;	// Initiate TX.
///          }
///
/// Example of usage : The codes example below illustrates how to retrieve 1 byte of data from
///                    the USART receive buffer.
///			if (gSCIstatus2.bRXRDY == 1)	// Check if USART receive any data.
///		    {
///             if (gSCIstatus2.bRXOVF == 0) // Make sure no overflow error.
///			    {
///					bytData = gbytRXbuffer2[0];	// Get 1 byte and ignore all others.
///             }
///             else
///             {
///					gSCIstatus2.bRXOVF = 0; 	// Reset overflow error flag.
///             }
///             gSCIstatus2.bRXRDY = 0;	// Reset valid data flag.
///             gbytRXbufptr2 = 0; 		// Reset pointer.
///             PIN_LED2_CLEAR;			// Turn off indicator LED2.
///			}
///
/// Note: Another way to check for received data is to monitor the received buffer pointer
/// gbytRXbufptr2.  If no data this pointer is 0, a value greater than zero indicates the
/// number of bytes contain in the receive buffer.


void Proce_USART0_Driver(TASK_ATTRIBUTE *ptrTask)
{
	int nTemp;
	int ni;

	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - USART0 Initialization.
				// Setup IO pins mode and configure the peripheral pins:

				// Setup pin PB0 as input. General purpose input with internal pull-up.
				PIOB->PIO_PPDDR |= PIO_PPDDR_P0;				// Disable internal pull-down to PB0.
				PIOB->PIO_PUER |= PIO_PUER_P0;					// Enable internal pull-up to PB0.
				//PIOB->PIO_PER |= PIO_PER_P0;					// PB0 is controlled by PIO.
				PIOB->PIO_ODR |= PIO_ODR_P0;					// Disable output write to PB0.
				//PIOB->PIO_IFER |= PIO_IFER_P0;				// Enable input glitch filter to PB0. This is optional.
 				
				// 24 Nov 2015: To enable a peripheral, we need to:
 				// 1. Assign the IO pins to the peripheral.
 				// 2. Select the correct peripheral block (A, B, C or D).
 				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P0;	// Set PB0 and PB1 to be controlled by Peripheral.
 				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P1;	// USART0 resides in Peripheral block C, with 
																// PB0 = RXD0 and PB1 = TXD0.
 																
 				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P0;	// Select peripheral block C for
 				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) | PIO_ABCDSR_P0;	// PB0.
 				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P1;	// Select peripheral block C for
 				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) | PIO_ABCDSR_P1;	// PB1.		 
				 
				PMC->PMC_PCR = PMC_PCR_EN | PMC_PCR_CMD | PMC_PCR_PID(13);		// Enable peripheral clock to USART0 (ID13)

				// Setup baud rate generator register.
				// Baudrate = (Peripheral clock)/(8(2-Over)CD)
				// Here Over = 1.				
				USART0->US_MR |= US_MR_OVER;
				USART0->US_BRGR = (__FPERIPHERAL_MHz*1000)/(8*_USART_BAUDRATE_kBPS);
				
				// Setup USART0 operation mode:
				// 1. USART mode = Normal.
				// 2. Peripheral clock is selected.
				// 3. 8 bits data, no parity, 1 stop bit.
				// 4. No interrupt.
				// 5. Channel mode = Normal.
				// 6. 8x Oversampling (Over = 1).
				// 7. The NACK is not generated.
				// 8. Start frame delimiter is one bit.
				// 9. Disable Manchester encoder/decoder. 
				USART0->US_MR |= US_MR_USART_MODE_NORMAL | US_MR_CHRL_8_BIT | US_MR_PAR_NO | US_MR_ONEBIT;
							 
				USART0->US_CR = US_CR_TXEN;						// Enable transmitter.             
			    USART0->US_CR = USART0->US_CR | US_CR_RXEN;		// Enable receiver.           
						   
				gbytTXbuflen2 = 0;								// Initialize all relevant variables and flags.
				gbytTXbufptr2 = 0;								// Clear transmit buffer 2 pointer.
				gSCIstatus2.bRXRDY = 0;	
				gSCIstatus2.bTXRDY = 0;
				gSCIstatus2.bRXOVF = 0;
                                
				gbytRXbufptr2 = 0;								// Clear receive buffer 2 pointer.
                PIN_LED2_CLEAR;									// Off indicator LED2.
			
				OSSetTaskContext(ptrTask, 1, 100);				// Next state = 1, timer = 100.
			break;
			
			case 1: // State 1 - Initialization of XDMAC Channel 2, map to USART0 TX holding buffer.
			// Setup XDMAC Channel 2 to handle transfer of data from SRAM to USART0 US_THR (Transmit holding register).
			// For more info see Atmel Application Note 42761A - 2016. Here's how it works:
			//
			// Memory (micro-block size) -> Peripheral (in chunk)
			//
			// Channel allocation: 2.
			// Source: SRAM.
			// Destination:  USART0 TX (XDMAC_CC.PERID = 7).
			// Transfer mode (TYPE): Single block with single micro-block. (BLEN = 0)
			// Memory burst size (MBSIZE): 1
			// Chunk size (CSIZE): 1 chunks
			// Channel data width (DWIDTH): byte.
			// Source address mode (SAM): increment.
			// Destination address mode (DAM): fixed.
			
				nTemp = XDMAC->XDMAC_CHID[2].XDMAC_CIS;			// Clear channel 2 interrupt status register.  This is a read-only register, reading it will clear all
																// interrupt flags.
				XDMAC->XDMAC_CHID[2].XDMAC_CSA = (uint32_t) gbytTXbuffer2;	// Set source start address.
				XDMAC->XDMAC_CHID[2].XDMAC_CDA = (uint32_t) &(USART0->US_THR);	// Set destination start address.

				XDMAC->XDMAC_CHID[2].XDMAC_CUBC = XDMAC_CUBC_UBLEN(1);	// Set the number of data chunks in a micro-block, default.  User
																		// to modify later.
				XDMAC->XDMAC_CHID[2].XDMAC_CC = XDMAC_CC_TYPE_PER_TRAN|	// Peripheral synchronized mode.
				XDMAC_CC_CSIZE_CHK_1|						// Chunk size in terms of data unit, 1 byte, halfword (16-bits) or word (32-bits). 
															// For USART0 since US_THR is 1 byte deep, we set chunk size to 1.
				XDMAC_CC_MBSIZE_SINGLE|						// Memory burst size, , not used in memory-to-peripheral transfer.
				XDMAC_CC_DSYNC_MEM2PER|						// Memory to peripheral.
				XDMAC_CC_DWIDTH_BYTE|						// Data unit size = byte.
				XDMAC_CC_SIF_AHB_IF0|						// Data is read through this AHB Master interface, connects to SRAM.
				XDMAC_CC_DIF_AHB_IF1|						// Data is write through this AHB Master interface, connects to peripheral bus.
				XDMAC_CC_SAM_INCREMENTED_AM|				// Source address, automatic increment.
				XDMAC_CC_DAM_FIXED_AM|						// Destination address, fixed. For TX we increment the source address.
				XDMAC_CC_SWREQ_HWR_CONNECTED|	// Hardware request line is connected to the peripheral request line, i.e. peripheral trigger the DMA transfer.
				XDMAC_CC_PERID(7);				// Connect to USART0 TX.  See datasheet.
			
				XDMAC->XDMAC_CHID[2].XDMAC_CNDC = 0;		// Next descriptor control register.
				XDMAC->XDMAC_CHID[2].XDMAC_CBC = 0;			// Block control register.
				XDMAC->XDMAC_CHID[2].XDMAC_CDS_MSP = 0;		// Data stride memory set pattern register.
				XDMAC->XDMAC_CHID[2].XDMAC_CSUS = 0;		// Source microblock stride register.
				XDMAC->XDMAC_CHID[2].XDMAC_CDUS = 0;		// Destination microblock stride register.
			
				OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
			break;
			
			case 2: // State 2 - Transmit and receive buffer manager.
	
				// Check for data to send via USART.
				// Note that the transmit buffer is only 2-level deep in ARM Cortex-M4 micro-controllers.				
				if (gSCIstatus2.bTXRDY == 1)					// Check if valid data in SCI buffer.
				{
					if (gSCIstatus2.bTXDMAEN == 0)				// Transmit without DMA.
					{
						while ((USART0->US_CSR & US_CSR_TXRDY) > 0)	// Check if USART transmit holding buffer is not full.
						{
							PIN_LED2_SET;						// On indicator LED2.
							if (gbytTXbufptr2 < gbytTXbuflen2)	// Make sure we haven't reach end of valid data to transmit.
							{
								USART0->US_THR = gbytTXbuffer2[gbytTXbufptr2];	// Load 1 byte data to USART transmit holding buffer.
								gbytTXbufptr2++;                // Pointer to next byte in TX buffer.
							}
							else                                // End of data to transmit.
							{
								gbytTXbufptr2 = 0;              // Reset TX buffer pointer.
								gbytTXbuflen2 = 0;              // Reset TX buffer length.
								gSCIstatus2.bTXRDY = 0;         // Reset transmit flag.
								PIN_LED2_CLEAR;                 // Off indicator LED2.
								break;
							} // if (gbytTXbufptr2 < gbytTXbuflen2)
						} // while ((USART0->US_CSR & US_CSR_TXRDY) > 0)
					} // if (gSCIstatus2.bTXDMAEN == 0)
					else										// Transmit with DMA.
					{
						if ((XDMAC->XDMAC_GS & XDMAC_GS_ST2_Msk) == 0)	// Check if DMA Channel 2 (USART0 TX) transmit is completed.
						{
							gSCIstatus2.bTXRDY = 0;				// Reset transmit flag.
							PIN_LED2_CLEAR;						// Off indicator LED2.
						}
					}
				}
										
				// Check for data to receive via USART.
				// Note that the receive FIFO buffer is only 2-level deep in ARM Cortex-M4 micro-controllers.
                // Here we ignore Parity error.  If overflow or framing error is detected, we need to write a 1 
				// to the bit RSTSTA to clear the error flags.  It is also advisable to reset the receiver.
                                
				if (((USART0->US_CSR & US_CSR_FRAME) == 0) && ((USART0->US_CSR & US_CSR_OVRE) == 0)) 
                {															// Make sure no hardware overflow and 
																			// and framing error.
                    while ((USART0->US_CSR & US_CSR_RXRDY) > 0)				// Check for availability of data received.
																			// available at UART
                    {
                        PIN_LED2_SET;										// On indicator LED2.
                        if (gbytRXbufptr2 < __SCI_RXBUF2_LENGTH)			// check for data overflow.
                        {													// Read a character from USART.
                            gbytRXbuffer2[gbytRXbufptr2] = USART0->US_RHR;	// Get received data byte.
                            gbytRXbufptr2++;								// Pointer to next byte in RX buffer.
                            gSCIstatus2.bRXRDY = 1;							// Set valid data flag.
                        }
                        else 												// data overflow.
                        {
                            gbytRXbufptr2 = 0;								// Reset buffer pointer.
                            gSCIstatus2.bRXOVF = 1;							// Set receive data overflow flag.
                        }
                        
                    }
                }
                else														// Hard overflow or/and framing error.
                {
                    USART0->US_CR = USART0->US_CR | US_CR_RSTSTA;			// Clear overrun and framing error flags.
					UART0->UART_CR = UART0->UART_CR | UART_CR_RSTRX;		// Reset the receiver.		
                    gbytRXbufptr2 = 0;										// Reset buffer pointer.
                    gSCIstatus2.bRXOVF = 1;									// Set receive data overflow flag.
					PIN_LED2_CLEAR;
                } 
				
				OSSetTaskContext(ptrTask, 2, 1); // Next state = 2, timer = 1.
			break;

			default:
				OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}


