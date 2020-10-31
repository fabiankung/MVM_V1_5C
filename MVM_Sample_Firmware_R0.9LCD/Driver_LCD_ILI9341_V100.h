// Author			: Fabian Kung
// Date				: 31 Oct 2020
// Filename			: Driver_LCD_ILI9340_V100.h

#ifndef _DRIVER_SERIAL_LCD_ILI9341_H
#define _DRIVER_SERIAL_LCD_ILI9341_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//
#define		__QQVGA__				// Comment this out. We want QVGA resolution.
									// IMPORTANT: To also comment this definition out in the
									// corresponding C file.
			
//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_LCD_ILI9341_Driver(TASK_ATTRIBUTE *);

#endif
