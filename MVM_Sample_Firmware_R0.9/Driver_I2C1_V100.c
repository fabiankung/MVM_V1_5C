//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT)
//
//  (c) Copyright 2018, Fabian Kung Wai Lee, Selangor, MALAYSIA
//  All Rights Reserved  
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_I2C1_V100.c
// Author(s)		: Fabian Kung
// Last modified	: 11 July 2018
// Toolsuites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//					  ARM CMSIS 5.0.1

#include "osmain.h"

// NOTE: Public function prototypes are declared in the corresponding *.h file.

//
//
// --- PUBLIC VARIABLES ---
//

// Data buffer and address pointers for wired serial communications.
#define     __MAX_I2C_DATA_BYTE               16     // Number of bytes for I2C receive and transmit buffer.
#define     __I2C_TIMEOUT_COUNT               25    // No. of system ticks before the I2C routine timeout during
                                                    // read data stage.
#define     __I2C_BAUD_RATE_MHZ               0.1   // 100 kHz (Standard rate)

I2C_STATUS  gI2CStat;                   // I2C status.
uint8_t     gbytI2CSlaveAdd;            // Slave address (7 bit, from bit0-bit6).
uint8_t     gbytI2CRegAdd;              // Slave register address.
uint8_t     gbytI2CByteCount;           // No. of bytes to read or write to Slave.
uint8_t     gbytI2CRXbuf[__MAX_I2C_DATA_BYTE];                // Data read from Slave register.
uint8_t     gbytI2CTXbuf[__MAX_I2C_DATA_BYTE];               // Data to write to Slave register.

///
/// Function name	: Proce_I2C1_Driver
///
/// Author			: Fabian Kung
///
/// Last modified	: 1 June 2018
///
/// Code Version	: 0.80
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS		: 1. Pin PB5 = TWCK1, peripheral A, output.
///               2. Pin PB4 = TWD1, peripheralA, input/output.
///
/// MODULES		: 1. TWIHS1 (Internal) on Peripheral A.  Clock from peripheral clock.
///
/// RTOS		: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    :

#ifdef __OS_VER			// Check RTOS version compatibility.
	#if __OS_VER < 1
		#error "Proce_I2C1_Driver: Incompatible OS version"
	#endif
#else
	#error "Proce_I2C1_Driver: An RTOS is required with this function"
#endif

///
/// Description	:
/// This is a driver routines for TWIHS module of the ATSAMS70 processor.  
/// This driver handles the low-level transmit and receive
/// operations. It assumes a single Master (i.e. this processor) and multiple slaves
/// environment.

/// I2C bus properties:
/// Baud rate = 100 kHz (standard rate) and 400 kHz (fast rate).
/// Mode: Single Master.
///
/// --- Example of usage: Transmit operation ---
/// Suppose we want to update the registers of a Slave device:
/// Register address 0x20: data = 0xFA
/// Register address 0x21: data = 0xCD
/// The Slave device has an address 0x1E.
///
/// The codes as follows:
/// if (gI2CStat.bI2CBusy == 0)     // Make sure I2C module is not being used.
/// {
///     gbytI2CByteCount = 2;       // Indicate no. of bytes to transmit.
///     gbytI2CRegAdd = 0x20;       // Start address of register.
///     gbytI2CTXbuf[0] = 0xFA;
///     gbytI2CTXbuf[1] = 0xCD;
///     gbytI2CSlaveAdd =  0x1E;
///     gI2CStat.bSend = 1;
/// }
/// The user routine can monitor the flag gI2CStat.bI2CBusy or gI2CStat.bSend.  Once the
/// transmission is completed, both flags will be cleared by the driver.
///
/// --- Example of usage: Receive operation ---
/// Suppose we want to receive 1 byte from the Slave device:
/// Register address 0x30.
/// Slave device address: 0x1E.
///
/// if (gI2CStat.bI2CBusy == 0)     // Make sure I2C module is not being used.
/// {
///     gbytI2CByteCount = 1;       // Indicate no. of bytes to read.
///     gbytI2CRegAdd = 0x30;       // Start address of register.
///     gbytI2CSlaveAdd =  0x1E;
///     gI2CStat.bRead = 1;
/// }
/// The user routine can monitor the flag gI2CStat.bI2CBusy or gI2CStat.bRead.  Once the
/// transmission is completed, both flags will be cleared by the driver.  The received
/// data will be stored in gbytI2CRXbuf[0].
/// if (gI2CStat.bRead == 0)       // Check if Read operation is completed.
/// {                              // Read operation complete, check received data.
///     User codes here
/// }
/// else if (gI2CStat.bCommError == 1)  // Check for I2C bus error.
/// {
/// }

void Proce_I2C1_Driver(TASK_ATTRIBUTE *ptrTask)
{
    static int nIndex = 0;
    //static int nTimeOut = 0;
    static int nCount = 0;

    if (ptrTask->nTimer == 0)
    {
		switch (ptrTask->nState)
		{
            case 0: // State 0 - Initialization of I2C2 module and set as Master mode.
                gI2CStat.bCommError = 0;                // Clear error flag.
                gI2CStat.bI2CBusy = 1;                  // Initially indicate I2C module is busy.
                gbytI2CRXbuf[0] = 0;					// After a short delay we will clear the busy flag.
                gbytI2CRegAdd = 0;
                gI2CStat.bSend = 0;
                gI2CStat.bRead = 0;
				
				// 1 June 2018: In ATSAMS70, pins PB4 and PB5 are also used as TDI and TDO/TRACESWO debug pins by default.
				// To use PB4 and PB5 as normal IO pins, we need to set the bus matrix system I/O configuration.
				MATRIX->CCFG_SYSIO = (MATRIX->CCFG_SYSIO) | CCFG_SYSIO_SYSIO4;	// Disable TDI function, enable PB4 function.
				MATRIX->CCFG_SYSIO = (MATRIX->CCFG_SYSIO) | CCFG_SYSIO_SYSIO5;	// Disable TDO/TRACESWI function, enable PB5 function.
				
																// 24 Nov 2015: To enable a peripheral, we need to:
																// 1. Assign the IO pins to the peripheral.
																// 2. Select the correct peripheral block (A, B, C or D).
				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P4;	// Set PB4 and PB5 to be controlled by Peripheral.
				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P5;	// TWI1 (Two-wire Interface 1) resides in
																// in Peripheral block A, with PB4 = SDA and
																// PB5 = SCL. 
				
				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P4;	// Select peripheral block A for
				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) & ~PIO_ABCDSR_P4;	// PB4.
				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) & ~PIO_ABCDSR_P5;	// Select peripheral block A for
				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) & ~PIO_ABCDSR_P5;	// PB5.	
							
				TWIHS1->TWIHS_MMR = TWIHS_MMR_DADR(gbytI2CSlaveAdd);	// Set Slave device address (7-bits).
				
				// 1 June 2018: Set clock waveform.
				// Here we are setting the clock to 100 kHz.  Each clock cycle from SCL line is divided into
				// low (tLow) and high (tHigh) periods, which should add up to 10 usec for 100 kHz baud rate.    
				// Thus assuming 50% duty cycle: tLow = 5 usec, tHigh = 5 usec.
				//
				// Where (see datasheet):
				//
				//                   CKDIV
				// tLow = ((CLDIV x 2     )  + 3) x tPeripheral
				//                    CKDIV
				// tHigh = ((CHDIV x 2    )  + 3) x tPeripheral
				//
				// If tPheripheral = 6.667 nsec (for 150 MHz peripheral clock), then the following will produce
				// clock frequency near to 100 kHz.
				// CLDIV = CHDIV = 93
				// CKDIV = 3
				// Actual frequency = 100.357 kHz
				// Percentage of error = 0.357%
				//TWIHS1->TWIHS_CWGR = (TWIHS1->TWIHS_CWGR) & (~TWIHS_CWGR_CKSRC);	// Select peripheral clock to generate TWIHS baud rate (This is not recognized in CMSIS 5.0.1 and above).
				TWIHS1->TWIHS_CWGR = (TWIHS1->TWIHS_CWGR) | TWIHS_CWGR_CLDIV(93) | TWIHS_CWGR_CHDIV(93);
				TWIHS1->TWIHS_CWGR = (TWIHS1->TWIHS_CWGR) | TWIHS_CWGR_CKDIV(3);
				TWIHS1->TWIHS_CR = (TWIHS1->TWIHS_CR) | TWIHS_CR_SVDIS;	// Disable Slave mode.
				TWIHS1->TWIHS_CR = (TWIHS1->TWIHS_CR) | TWIHS_CR_MSEN;	// Enable the Master mode. 
				PMC->PMC_PCER0 |= PMC_PCER0_PID20;		// Enable peripheral clock to TWIHS1 (ID20)
				OSSetTaskContext(ptrTask, 49, 30*__NUM_SYSTEMTICK_MSEC);     // Next state = 49, timer = 30 msec.
            break;

            case 1: // State 1 - Dispatcher.
                if (gI2CStat.bRead == 1)                  // Reading data from Slave.
                {
                    gI2CStat.bI2CBusy = 1;                // Indicate I2C module is occupied.
                    OSSetTaskContext(ptrTask, 30, 1);     // Next state = 30, timer = 1.
                }
                else if (gI2CStat.bSend == 1)             // Transmission of data to Slave.
                {
                    gI2CStat.bI2CBusy = 1;                // Indicate I2C module is occupied.
                    OSSetTaskContext(ptrTask, 45, 1);     // Next state = 45, timer = 1.
                }
                else
                {
                    OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
                }
                break;

            // --- Multi-bytes read (codes from dsPIC33E micro-controller) ---
            case 30: // State 30 - Assert Start condition.
                //I2C2CONbits.SEN = 1;                      // Assert Start condiction on I2C bus.
                //nCount = 0;                              // Reset counter.
                //OSSetTaskContext(ptrTask, 31, 1);        // Back to state = 31, timer = 1.
				gI2CStat.bRead = 0;
				OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
            break;

/*
           case 31: // State 31 - Tx slave device address, write mode.
                I2C2TRN = gbytI2CSlaveAdd<<1;              // Data = Slave address with R/W bit = 0 (Master is writing to the Slave).
                OSSetTaskContext(ptrTask, 32, 1);        // Back to state = 32, timer = 1.
           break;

           case 32: // State 32 - Check status of transmission.
                if (I2C2STATbits.TRSTAT == 1)           // Check if Master transmission is still in progress.
                {
                    OSSetTaskContext(ptrTask, 32, 1);    // Back to state = 32, timer = 1.
                }
                else                                    // Transmission end, check acknowledgment from Slave.
                {
                    if (I2C2STATbits.ACKSTAT == 0)      // ACK received from Slave, continue.
                    {
                        gI2CStat.bCommError = 0;
                        OSSetTaskContext(ptrTask, 33, 1);    // Next state = 33, timer = 1.
                    }
                    else                                // NAK received from Slave, end.
                    {
                        gI2CStat.bCommError = 1;        // Set communication error flag.
                        OSSetTaskContext(ptrTask, 41, 1);    // Next state = 41, timer = 1.
                    }    
                }
                break;

           case 33: // State 33 - TX start register to read.
                I2C2TRN = gbytI2CRegAdd;                 // Send register address to read.
                OSSetTaskContext(ptrTask, 34, 1);        // Next state = 34, timer = 1.
                break;

           case 34: // State 34 - Check status of transmission.
                if (I2C2STATbits.TRSTAT == 1)           // Check if Master transmission is still in progress.
                {
                    OSSetTaskContext(ptrTask, 34, 1);   // Back to state = 34, timer = 1.
                }
                else                                    // Transmission end, check acknowledge from Slave.
                {
                    if (I2C2STATbits.ACKSTAT == 0)      // ACK received from Slave, continue.
                    {
                        gI2CStat.bCommError = 0;
                        OSSetTaskContext(ptrTask, 35, 1);    // Next state = 35, timer = 1.
                    }
                    else                                // NAK received from Slave, end.
                    {
                        gI2CStat.bCommError = 1;        // Set communication error flag.
                        OSSetTaskContext(ptrTask, 41, 1);    // Next state = 41, timer = 1.
                    }                   
                }
                break;

            case 35: // State 35 - Initiate Restart condition.
                I2C2CONbits.RSEN = 1;                   // Assert repeat start condition on I2C bus.
                OSSetTaskContext(ptrTask, 36, 1);        // Next state = 36, timer = 1.
                break;

           case 36: // State 36 - Tx slave device address, read mode.
                I2C2TRN = (gbytI2CSlaveAdd<<1) | 0x01;     // Data = Slave address with R/W bit = 1 (Master is reading from the Slave).
                OSSetTaskContext(ptrTask, 37, 1);        //  Next state = 37, timer = 1.
                break;

           case 37: // State 37 - Check status of transmission.
                if (I2C2STATbits.TRSTAT == 1)           // Check if Master transmission is still in progress.
                {
                    OSSetTaskContext(ptrTask, 37, 1);    // Back to state = 37, timer = 1.
                }
                else                                    // Transmission end, check acknowledge from Slave.
                {
                    if (I2C2STATbits.ACKSTAT == 0)      // ACK received from Slave, continue.
                    {
                        gI2CStat.bCommError = 0;
                        OSSetTaskContext(ptrTask, 38, 1);    // Next state = 38, timer = 1.
                    }
                    else                                // NAK received from Slave, end.
                    {
                        gI2CStat.bCommError = 1;        // Set communication error flag.
                        OSSetTaskContext(ptrTask, 41, 1);    // Next state = 41, timer = 1.
                    }
                    
                }
                break;

           case 38: // State 38 - Enable receive.
               I2C2CONbits.RCEN = 1;                    // Enable receive operation.
               nTimeOut = 0;                            // Reset timeout timer.
               OSSetTaskContext(ptrTask, 39, 1);        // Next state = 39, timer = 1.
               break;

           case 39: // State 39 - Check for receive buffer full status.
                if (I2C2STATbits.RBF == 1)              // Check if all bits are received.
                {
                    gbytI2CRXbuf[nCount] = I2C2RCV;      // Get received data.
                    OSSetTaskContext(ptrTask, 40, 1);   // Next state = 40, timer = 1.
                }
                else
                {
                    nTimeOut++;
                    if (nTimeOut > __I2C_TIMEOUT_COUNT)  // Check for timeout.
                    {
                        gI2CStat.bCommError = 1;        // Set communication error flag.
                        OSSetTaskContext(ptrTask, 40, 1);   // Next state = 40, timer = 1.
                    }
                    else
                    {
                        OSSetTaskContext(ptrTask, 39, 1);   // Next state = 39, timer = 1.
                    }
                }
                break;

            case 40: // State 40 - Master generates acknowledgment.
                gbytI2CByteCount--;
                nCount++;
                // Check of end of data to read, or the I2C receive buffer is full.
                if ((gbytI2CByteCount == 0) || (nCount == __MAX_I2C_DATA_BYTE))
                {
                    I2C2CONbits.ACKDT = 1;                   // Set NAK when receive data.
                    I2C2CONbits.ACKEN = 1;                   // Enable acknowledgement sequence.
                    OSSetTaskContext(ptrTask, 41, 1);        // Next state = 41, timer = 1.
                }
                else                                         // Still in reading mode.
                {
                    I2C2CONbits.ACKDT = 0;                   // Set ACK when receive data.
                    I2C2CONbits.ACKEN = 1;                   // Enable acknowledgement sequence.
                    OSSetTaskContext(ptrTask, 38, 1);        // Next state = 38, timer = 1.
                }
               break;

            case 41: // State 41 - End, initiate Stop condition on I2C bus.
                I2C2CONbits.PEN = 1;                    // Initiate Stop condition.
                OSSetTaskContext(ptrTask, 42, 1);        // Next state = 42, timer = 1.
                break;

            case 42: // State 42 - Tidy up.
                gI2CStat.bI2CBusy = 0;                  // I2C module is idle.
                gI2CStat.bRead = 0;
                OSSetTaskContext(ptrTask, 1, 1);        // Next state = 1, timer = 1.
                break;
*/
            // --- Multi-byte master write ---
            case 45: // State 45 - Reset the TWI module to Master write mode, and load Slave address.
                nCount = 0;											// Reset pointer.
				//TWIHS1->TWI_MMR = TWI_MMR_DADR(gbytI2CSlaveAdd<<1);	// Load Slave device address (7-bits).
				TWIHS1->TWIHS_MMR = TWIHS_MMR_DADR(gbytI2CSlaveAdd);	// Load Slave device address (7-bits).
                TWIHS1->TWIHS_MMR = TWIHS1->TWIHS_MMR & ~TWIHS_MMR_MREAD;	// Clear MREAD, TX mode.
                OSSetTaskContext(ptrTask, 46, 1);					// Next state = 46, timer = 1.
                break;

           case 46: // State 46 - TX start register to write to.  Note: Before sending the first byte,
					// a START condition will be asserted by the TWI Master.
                TWIHS1->TWIHS_THR = gbytI2CRegAdd;					// Send register address to update.
                OSSetTaskContext(ptrTask, 47, 1);					// Next state = 47, timer = 1.
           break;

           case 47: // State 47 - TX data.
				if ((TWIHS1->TWIHS_SR & TWIHS_SR_TXRDY) == 0)		// Check if data is still in holding register (TWI_THR).
                {
                    OSSetTaskContext(ptrTask, 47, 1);				// Next state = 47, timer = 1.
                }
				else  
				{													// Data-to-send is transferred to internal register.
					if (gbytI2CByteCount > nCount)					// Check if there is another data byte to transmit.
					{
						TWIHS1->TWIHS_THR = gbytI2CTXbuf[nCount];	// Send data.
						nCount++;									// Increment pointer.
						OSSetTaskContext(ptrTask, 47, 1);			// Next state = 47, timer = 1.
					}
					else
					{
						TWIHS1->TWIHS_CR = TWIHS1->TWIHS_CR | TWIHS_CR_STOP;	// Assert STOP condition.
						OSSetTaskContext(ptrTask, 48, 1);			// Next state = 48, timer = 1.
					}
					
				}	
                break;

           case 48: // State 48 - Check TXCOMP flag.
				if ((TWIHS1->TWIHS_SR & TWIHS_SR_TXCOMP) == 0)		// Check if Master transmission is still in progress.
				{
					OSSetTaskContext(ptrTask, 48, 1);				// Next state = 48, timer = 1.
				}
				else
				{													// Master transmission ends.
					OSSetTaskContext(ptrTask, 49, 1);				// Next state = 49, timer = 1.
				}
				break;  

            case 49: // State 49 - Tidy up.
                gI2CStat.bI2CBusy = 0;								// I2C module is idle.
                gI2CStat.bSend = 0;
                OSSetTaskContext(ptrTask, 1, 1);					// Next state = 1, timer = 1.
                break;
				
            default:
		OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
            break;
        }
    }
}