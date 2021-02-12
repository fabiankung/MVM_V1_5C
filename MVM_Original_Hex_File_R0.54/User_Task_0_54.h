// Author			: Fabian Kung
// Date				: 27 Nov 2019
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
void Proce_MessageLoop_StreamImage(TASK_ATTRIBUTE *);
void Proce_RunImageProcess(TASK_ATTRIBUTE *);
void ImageProcessingAlgorithm1(void);
void ImageProcessingAlgorithm2(void);
void ImageProcessingAlgorithm3(void);
