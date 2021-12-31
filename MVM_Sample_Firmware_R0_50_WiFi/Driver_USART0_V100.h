// Author			: Fabian Kung
// Date				: 18 Dec 2021
// Filename			: Driver_USART_V100.h

#ifndef _DRIVER_USART_SAMS70_H
#define _DRIVER_USART_SAMS70_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//

#define __SCI_TXBUF2_LENGTH      16			// SCI transmit  buffer2 length in bytes.
#define __SCI_RXBUF2_LENGTH      16			// SCI receive  buffer2 length in bytes.		
		
// Data buffer and address pointers for wired serial communications.
extern uint8_t gbytTXbuffer2[__SCI_TXBUF2_LENGTH-1];
extern uint8_t gbytTXbufptr2;
extern uint8_t gbytTXbuflen2;
extern uint8_t gbytRXbuffer2[__SCI_RXBUF2_LENGTH-1];
extern uint8_t gbytRXbufptr2;

extern	SCI_STATUS gSCIstatus2;
//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_USART0_Driver(TASK_ATTRIBUTE *);

#endif
