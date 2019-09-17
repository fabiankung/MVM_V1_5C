// Author			: Fabian Kung
// Date				: 11 July 2015
// Filename			: Driver_I2C1_V100.h

#ifndef _DRIVER_I2C_SAMS70_H
#define _DRIVER_I2C_SAMS70_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//
				
// Data buffer and address pointers for wired serial communications.
#define     __MAX_I2C_DATA_BYTE               16

extern  I2C_STATUS  gI2CStat;                   // I2C status.
extern  uint8_t     gbytI2CSlaveAdd;           // Slave address (7 bit, from bit0-bit6).
extern  uint8_t     gbytI2CRegAdd;              // Slave register address.
extern  uint8_t     gbytI2CByteCount;           // No. of bytes to read or write to Slave.
extern  uint8_t     gbytI2CRXbuf[__MAX_I2C_DATA_BYTE];                // Data read from Slave register.
extern  uint8_t     gbytI2CTXbuf[__MAX_I2C_DATA_BYTE];               // Data to write to Slave register.


//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_I2C1_Driver(TASK_ATTRIBUTE *);

#endif
