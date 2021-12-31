// Author			: Fabian Kung
// Date				: 10 Dec 2021
// Filename			: User_Task.h


// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

// 
//
// --- PUBLIC VARIABLES ---
//


//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_MessageLoop_StreamImage(TASK_ATTRIBUTE *);	// In file 'User_Task.c'
void Proce_Echo_UART2_RX(TASK_ATTRIBUTE *);				// In file 'User_Task.c'
void Proce_IPA1(TASK_ATTRIBUTE *);						// In file 'User_Task.c'

