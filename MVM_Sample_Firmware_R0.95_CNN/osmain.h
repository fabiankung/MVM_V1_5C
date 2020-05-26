/// Author			: Fabian Kung
/// Date			: 27 June 2018
/// Filename		: osmain.h

///////////////////////////////////////////////////////////////////////////////////////////////////
//  BEGINNING OF CODES SPECIFIC TO ARM CORTEX-M7 MICROCONTROLLER        ///////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "sam.h"
#include "sams70j20.h"
///////////////////////////////////////////////////////////////////////////////////////////////////
//  END OF CODES SPECIFIC TO ARM CORTEX-M7 MICROCONTROLLER        /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __OSMAIN_H
#define __OSMAIN_H

// --- Mapping of variable datatype to match the controller's architecture ---
// For ARM Cortex please refer to CMSIS file 'stdint.h' for the various 
// definitions integer type variables.
#define	INT16 	int					// 16-bits signed integer.
#define	UINT16	unsigned int		// 16-bits unsigned integer.
#define UINT32  unsigned long		// 32-bits unsigned integer.
#define	BYTE	unsigned char		// 8-bits unsigned integer.

// --- Micro-controller I/O Pin definitions ---
#define	PIN_OSPROCE1_SET        PIOA->PIO_ODSR |= PIO_ODSR_P27			// Set indicator LED1 driver pin, PA27.
#define	PIN_OSPROCE1_CLEAR		PIOA->PIO_ODSR &= ~PIO_ODSR_P27			// Clear indicator LED1 driver pin, PA27.

#define	PIN_LED2_SET			PIOA->PIO_ODSR |= PIO_ODSR_P24			// Set indicator LED2 driver pin, PA24.
#define	PIN_LED2_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P24		// Clear indicator LED2 driver pin, PA24.

// --- Processor Clock and Kernel Cycle in microseconds ---
// Note: Uncomment the required value for _TIMER1COUNT, and update the corresponding definition
// for the constant _SYSTEMTICK_US, in microseconds.

#define	__FOSC_MHz              300             // Oscillator clock frequency in MHz.
#define __FCORE_MHz             __FOSC_MHz*1.0  // Processor Core frequency HCLK = 300 MHz.
#define __FPERIPHERAL_MHz		__FOSC_MHz*0.5	// Processor Peripheral Clock or MCK frequency = 150 MHz.
#define	__TCLK_US               0.003333        // Minimum duration to execute 1 instruction
                                                // Tclk = 1/300000000 = 3.333 nsec.
#define __TCLK_PERI_US			0.006667		// Period for peripheral clock, 1/150000000 = 6.667 nsec.
                                                
#define __SYSTICKCOUNT          25000			// Note: 28 May 2018
												// The correct value should be 6250 according to the explanation in the 
												// datasheet and ARM Cortex M7 manual.  However I discover that
												// this value has to be multiplied by 4.
//#define __SYSTICKCOUNT          6250           // No. of Tcyc for SysTick to expire.  The SysTick
												// is triggered by the Processor Clock (HCLK) divided by 8.
												// SysTick Time Out = __SYSTICKCOUNT/(8xHCLK)
												// This value corresponds with 50000 single cycle 
												// instruction cycles executed by the ARM core.
												
#define	__SYSTEMTICK_US         166.67          // System_Tick = _SYSTICKCOUNT x Tclk_US x 8
									
#define __NUM_SYSTEMTICK_MSEC         6         // Requires 6 system ticks to hit 1 msec period.

///////////////////////////////////////////////////////////////////////////////////////////////////
//  END OF CODES SPECIFIC TO ARM CORTEX-M4 MICROCONTROLLER  //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


// --- RTOS CONSTANTS ---
#define	__OS_VER				2           // RTOS/Scheduler version, need to be integer (ANSI C preprocessor
											// expression requires integer). 

#define	__MAXTASK				12			// Maximum no. of concurrent tasks supported.

#define __SCI_TXBUF_LENGTH      170  		// SCI transmit  buffer length in bytes.
#define __SCI_RXBUF_LENGTH      8			// SCI receive  buffer length in bytes.

#define __SCI_TXBUF2_LENGTH      8			// SCI transmit  buffer2 length in bytes.
#define __SCI_RXBUF2_LENGTH      8			// SCI receive  buffer2 length in bytes.

// --- RTOS DATATYPES DECLARATIONS ---
// Type cast for a structure defining the attributes of a task,
// e.g. the task's ID, current state, counter, variables etc.
typedef struct StructTASK
{	
	int nID;      // The task identification.  Also determines the sequence
                    // in which the task is executed by the Kernel.  Task with 
                    // ID = 1 will be executed first. Valid value is 1-255.
                    // ID = 0 is used to indicate empty task.  
	int nState;	// The current state of the task.  Useful for implementing 
                    // an algorithmic state machine.
	int nTimer;	// This variable will be decremented on every clock tick. 
                    // The Scheduler uses this variable to determine whether to execute a 
                    // task or not.  If nTimer = 0, the corresponding task will be
                    // executed, else the task will be skipped.
                    // Useful for implementing a non-critical delay within a task.
} TASK_ATTRIBUTE;

// Type cast for a pointer to a task, TASK_POINTER with argument of TASK_ATTRIBUTE
typedef void (*TASK_POINTER)(TASK_ATTRIBUTE *);

// Type cast for a Bit-field structure - Serial Communication Interface (SCI) status
typedef struct StructSCI
{
	unsigned bTXRDY: 	1;	// Set to indicate valid data for the wired SCI module to transmit.
	unsigned bTXDMAEN:	1;	// Set to indicate to activate DMA for transmit operation.  This is useful
							// for transmitting large packet of data without intervention from the
							// micro-controller core.
	
	unsigned bRXRDY: 	1;	// Set if there is valid byte data in the wired SCI receive buffer.
	unsigned bRXOVF:	1;	// Set if there is data overflow in wired SCI receive buffer, i.e. old
                            // data has not been read but new data has arrived.
	unsigned bRFTXRDY:	1;	// Set to indicate valid data for the RF transceiver module to transmit.
	unsigned bRFRXRDY:	1;	// Set if there is valid byte data in the RF transceiver module receive 
                            // buffer.
	unsigned bRFRESET:	1;	// Set to reset RF transceiver.
	unsigned bRFTXERR:	1;	// Set to indicate transmission is not successful.
} SCI_STATUS;


// Type cast for Bit-field structure - I2C interface status.
typedef struct StructI2CStatus
{
    unsigned bI2CBusy:      1;      // Mutex, set when I2C module is being used.
	unsigned bCommError:    1;      // Set to indicate communication error in the I2C bus.
	unsigned bRead:         1;      // Set to initiate reading of data (Slave -> Master).
    unsigned bSend:         1;      // Set to initiate sending of data (Master -> Slave).
} I2C_STATUS;

// --- RTOS FUNCTIONS' PROTOTYPES ---
// Note: The body of the followings routines is in the file "os_APIs.c"
void OSInit(void);
int OSCreateTask(TASK_ATTRIBUTE *, TASK_POINTER );
void OSSetTaskContext(TASK_ATTRIBUTE *, int, int);
int OSTaskDelete(int);
void OSUpdateTaskTimer(void);
void OSEnterCritical(void);
void OSExitCritical(void);
void OSProce1(TASK_ATTRIBUTE *ptrTask); 	// Blink indicator LED1 process.
// Note: The body of the followings routines is in the file "os dsPIC33E_APIs.c"
void ClearWatchDog(void);
void SAMS70_Init(void);

// --- GLOBAL/EXTERNAL VARIABLES DECLARATION ---

// Note: The followings are defined in the file "os_APIs.c"
extern int gnRunTask;
extern int gnTaskCount;
extern unsigned int gunClockTick;
extern TASK_ATTRIBUTE gstrcTaskContext[__MAXTASK-1];
extern TASK_POINTER gfptrTask[__MAXTASK-1];
extern SCI_STATUS gSCIstatus;

// Note: The followings is defined in file "main.c"
extern int gnRunImage;
#endif
