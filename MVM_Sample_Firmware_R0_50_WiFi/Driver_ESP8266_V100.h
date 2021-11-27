// Author			: Fabian Kung
// Date				: 25 Nov 2021
// Filename			: Driver_ESP8266_V100.h

#ifndef _DRIVER_ESP8266_SAMS70_H
#define _DRIVER_ESP8266_SAMS70_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//
extern	int		gnESP8266Flag;			

//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_ESP8266_Driver(TASK_ATTRIBUTE *);
#endif
