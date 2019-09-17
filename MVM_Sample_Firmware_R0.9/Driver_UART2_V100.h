// Author			: Fabian Kung
// Date				: 11 July 2018
// Filename			: Driver_UART_V100.h

#ifndef _DRIVER_UART_SAMS70_H
#define _DRIVER_UART_SAMS70_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//
				
// Data buffer and address pointers for wired serial communications.
extern uint8_t gbytTXbuffer[__SCI_TXBUF_LENGTH-1];
extern uint8_t gbytTXbufptr;
extern uint8_t gbytTXbuflen;
extern uint8_t gbytRXbuffer[__SCI_RXBUF_LENGTH-1];
extern uint8_t gbytRXbufptr;


//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_UART2_Driver(TASK_ATTRIBUTE *);

#endif
