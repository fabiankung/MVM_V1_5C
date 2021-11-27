/////////////////////////////////////////////////////////////////////////////////////////////////////
///
///	MICROCONTROLLER INDEPENDENT APPLICATION PROGRAM INTERFACE ROUTINES
///
///  (c) Copyright 2015, Fabian Kung Wai Lee, Selangor, MALAYSIA
///  All Rights Reserved  
///   
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Filename         : os_APIs.c
/// Author           : Fabian Kung
/// Last updated     : 25 Nov 2014
/// File Version     : 1.09
/// Description      : This file contains the implementation of all the important routines
///                    used by the Kernel for task management. It include routines to create or
///                    initialize a task, delete a task from the Scheduler, setting a task's 
///                    context etc.  The routines are general and can be used for most 
///                    micro-controller families.

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one changes folder
#include "osmain.h"

// --- GLOBAL VARIABLES AND DATAYPES DECLARATION ---
volatile int gnRunTask;									// Flag to determine when to run tasks.
int gnTaskCount;								// Task counter.
volatile unsigned int gunClockTick;                      // Processor clock tick.
TASK_ATTRIBUTE gstrcTaskContext[__MAXTASK-1];   // Array to store task contexts.
TASK_POINTER gfptrTask[__MAXTASK-1];            // Array to store task pointers.

SCI_STATUS gSCIstatus;				// Status for UART and RF serial communication interface.

// --- RTOS FUNCTIONS ---

// Function name	: OSInit()
// Author			: Fabian Kung
// Last modified	: 20 Nov 2015
// Purpose			: Initialize the variables and parameters of the RTOS.
// Arguments		: None.
// Return			: None.
void OSInit()
{
	gunClockTick = 0; 		// Initialize 32-bits RTOS global timer.
}

/// Function name	: OSTaskCreate()
/// Author			: Fabian Kung
/// Last modified	: 20 Nov 2015
/// Purpose			: Add a new task to the OS's scheduler.
/// Arguments		: ptrTaskData = A pointer to the structure structTASK.
///                   ptrTask = a valid pointer to a user routine.
/// Return			: 0 if success, 1 or >0 if not successful.
/// Others			: Increment global variable gnTaskCount.
int OSCreateTask(TASK_ATTRIBUTE *ptrTaskData, TASK_POINTER ptrTask)
{
	if (gnTaskCount> __MAXTASK) 
	{
		return 1;                               // Maximum tasks exceeded.
	}
	else
	{
		ptrTaskData->nState = 0;		// Initialize the task's state and timer variables.
		ptrTaskData->nTimer = 1;
											
		gfptrTask[gnTaskCount] = ptrTask;	// Assign task's address to function pointer array.
		gnTaskCount++; 				// Increment task counter.
							// Initialize the task's ID
		ptrTaskData->nID = gnTaskCount; 	// Task's ID = current Task Count + 1.

		return 0;
	}
}

/// Function name	: OSSetTaskContext()
/// Author			: Fabian Kung
/// Last modified	: 20 Nov 2015
/// Purpose			: Set the task's State and Timer variables.
/// Arguments		: ptrTaskData = A pointer to the structure structTASK.
///					  nState = Next state of the task.
///					  nTimer = Timer, the no. of clock ticks before the task
///					  executes again.
/// Return			: None.
void OSSetTaskContext(TASK_ATTRIBUTE *ptrTaskData, int nState, int nTimer)
{
	ptrTaskData->nState = nState;
	ptrTaskData->nTimer = nTimer;
}

/// Function name	: OSTaskDelete()
/// Author			: Fabian Kung
/// Last modified	: 20 Nov 2015
/// Description		: Delete a task from the OS's scheduler.
/// Arguments		: nTaskID = An integer indicating the task ID.
/// Return			: 0 if success, 1 or >0 if not successful.
int OSTaskDelete(int nTaskID)
{
	int ni;

	if (nTaskID < gnTaskCount) // nTaskID can be from 1 to (gnTaskCount-1)
	{
		if (nTaskID < (gnTaskCount - 1))	// Shift all the elements of the gstrcTaskContext
		{					// array down 1 position.
			ni = nTaskID - 1;
			while (ni < (gnTaskCount - 1))
			{
				gstrcTaskContext[ni].nState = gstrcTaskContext[ni + 1].nState;
				gstrcTaskContext[ni].nTimer = gstrcTaskContext[ni + 1].nTimer;
				gstrcTaskContext[ni].nID = gstrcTaskContext[ni + 1].nID;
				gfptrTask[ni] = gfptrTask[ni + 1];
				ni++;
			}
			gnTaskCount--; 			// There is 1 less task to execute now.
		}
		return 0;
	}
	else
	{
		return 1;
	}
}

/// Function name	: OSUpdateTaskTimer()
/// Author			: Fabian Kung
/// Last modified	: 20 Nov 2015
/// Description		: Update the timer attribute of each task.
/// Arguments		: None.
/// Return			: None.
void OSUpdateTaskTimer(void)
{
	int ni = 0;

	ni = 0; 					// Reset index.
	while (ni < gnTaskCount)
	{	
		--(gstrcTaskContext[ni].nTimer); 	// Decrement the timer of each task.
		ni++; 					// Next task.
	}
}

