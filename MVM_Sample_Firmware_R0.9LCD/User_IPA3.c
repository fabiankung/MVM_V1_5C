//
// File				: User_IPA3.c
// Author(s)		: Fabian Kung
// Last modified	: 31 Oct 2020
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.0.1

#include "./osmain.h"
#include "./Driver_USART0_V100.h"
#include "./Driver_TCM8230_LCD.h"

//extern unsigned int	gunIPA3_Argument;
//extern	int		gnImageProcessingAlgorithmBusy;

//#define		_IMAGEPROCESSINGALGORITHM_BUSY	1
//#define		_IMAGEPROCESSINGALGORITHM_IDLE	0

extern	unsigned int	gunHue;					// For debug purpose.
unsigned int			gunIPA3_Argument = 0;	// 0 to recognize yellow-green object.
												// 1 to recognize red object.
												// 2 to recognize light green object.
												// else to recognize blue object.

typedef struct StructRec
{
	unsigned int nHeight;				// Height in pixels.
	unsigned int nWidth;				// Width in pixels.
	unsigned int nX;					// X coordinate of top left corner.
	unsigned int nY;					// y coordinate of top left corner.
	unsigned int nColor;				// Bit7 = 1 - No fill.
	// Bit7 = 0 - Fill.
	// Bit0-6 = 1 to 127, determines color.
	// If nColor = 0, the ROI will not be displayed in the remote monitor.
} MARKER_RECTANGLE;

extern	MARKER_RECTANGLE	gobjRec1;			// Marker objects.
extern	MARKER_RECTANGLE	gobjRec2;


///
/// Function name	: ImageProcessingAlgorithm3
///
/// Author			: Fabian Kung
///
/// Last modified	: 31 Oct 2020
///
/// Code Version	: 0.84
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			: None.
///
/// MODULES			: Camera driver.
///                   Proce_USART_Driver
///
/// RTOS			: Not needed.
///
/// Global Variables    : gnImageProcessingAlgorithmBusy
///                       gnValidFrameBuffer
///                       gunImgAtt[][] and gunImgAtt2[][]
///                       gnCameraLED
///
///
/// Description	:
/// Color object location.  Here we are loosely base on the algorithm described by T. Braunl, "Embedded robotics", 2nd edition,
/// 2006, Springer, with some improvement by F. Kung.
/// In the first iteration, the algorithm performs a search along each column for pixel matching certain color (hue).
/// These pixels are marked.  A second iteration then sift through the marked pixels, searching for interior marked pixels, i.e.
/// a marked pixel surrounded by 4 other marked pixels on the top, bottom, left and right.  The row and column number of each
/// interior marked pixel will be updated in row and column histogram registers.  Finally the 3rd iteration will search through
/// the histogram to find the row and column with highest interior marked pixels.  The second iteration represents an improvement
/// over the original algorithm by Brauhl.  Since in a real environment there are 'color noise', e.g. small or far objects with
/// matching the target hue, which cause the sporadic pixels in the background to be marked.  Applying the condition of
/// matched interior pixel eliminates most of these sporadic pixels.  Allowing the algorithm to zoom in to the correct target.


#define		_IP3_HUEOFINTEREST_HIGHLIMIT_RED			360		// Between 0 to 359, refer to hue table.
#define		_IP3_HUEOFINTEREST_LOWLIMIT_RED				339		//
#define		_IP3_HUEOFINTEREST_HIGHLIMIT_LIGHTGREEN		111		// Between 0 to 359, refer to hue table.
#define		_IP3_HUEOFINTEREST_LOWLIMIT_LIGHTGREEN		90		//
#define		_IP3_HUEOFINTEREST_HIGHLIMIT_BLUE			230		// Between 0 to 359, refer to hue table.
#define		_IP3_HUEOFINTEREST_LOWLIMIT_BLUE			209		//
#define		_IP3_HUEOFINTEREST_HIGHLIMIT_YELLOWGREEN	71		// Between 0 to 359, refer to hue table.
#define		_IP3_HUEOFINTEREST_LOWLIMIT_YELLOWGREEN		49		//
//#define		_IP3_VALID_PIXEL_THRESHOLD		3
#define		_IP3_VALID_PIXEL_THRESHOLD		2
//#define		_IP3_VALID_PIXEL_THRESHOLD		1

void ImageProcessingAlgorithm3(TASK_ATTRIBUTE *ptrTask)
{
	//static	int	nTimer = 1;		// Start timer with 1.
	//static	int nState = 0;
	
	static	int		nCurrentFrame;
	static	int		nXindex, nYindex;
	static	unsigned int	unHOIHigh, unHOILow;
	unsigned int	unHueTemp;
	unsigned int    unPixelAttrTemp;
	int				nIndex, nTemp;
	unsigned int	unResult;
	unsigned int	unPixel;
	unsigned int	unPixel1;
	unsigned int	unPixel3;
	unsigned int	unPixel5;
	unsigned int	unPixel7;
	static int nCounter, nHueCounterCol;
	static int nMaxCol, nMaxRow;
	static int nMaxValueCol, nMaxValueRow;
	static	int		nHOIHistoCol[_IMAGE_HRESOLUTION];		// Hue-of-interest (HOI) histogram for each column.
	static  int		nHOIHistoRow[_IMAGE_VRESOLUTION];		// Hue-of-interest (HOI) histogram for each row.
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Power-up initialization.
			//nState = 1;					// Next state = 1, delay = 1 tick.
			//nTimer = 1;
			OSSetTaskContext(ptrTask, 1, 1);							// Next state = 1, timer = 1.
			break;

			case 1: // State 1 - Initialization at the start of new image frame in the buffer.
			if (gnFrameCounter != nCurrentFrame)							// Check if new image frame has been captured.
			{
				nCurrentFrame = gnFrameCounter;								// Update current frame counter.			
				//PIN_FLAG4_SET;											// Set indicator flag, this is optional, for debugging purpose.
				nXindex = 1;												// Ignore 1st Column (i.e. Column 0).
				for (nIndex = 0; nIndex < _IMAGE_HRESOLUTION-1; nIndex++)	// Clear the HOI column histogram.
				{
					nHOIHistoCol[nIndex] = 0;
				}
				for (nIndex = 0; nIndex < _IMAGE_VRESOLUTION-1; nIndex++)	// Clear the HOI row histogram.
				{
					nHOIHistoRow[nIndex] = 0;
				}
				switch (gunIPA3_Argument)									// Check for the hue to identify.
				{
					case 1: // Red.
					unHOIHigh = _IP3_HUEOFINTEREST_HIGHLIMIT_RED<<_HUE_SHIFT;			// Set the upper and lower range of the
					unHOILow = _IP3_HUEOFINTEREST_LOWLIMIT_RED<<_HUE_SHIFT;				// hue of interest.
					break;
				
					case 2: // Light green.
					unHOIHigh = _IP3_HUEOFINTEREST_HIGHLIMIT_LIGHTGREEN<<_HUE_SHIFT;	// Set the upper and lower range of the
					unHOILow = _IP3_HUEOFINTEREST_LOWLIMIT_LIGHTGREEN<<_HUE_SHIFT;		// hue of interest.
					break;
				
					case 3: // Blue
					unHOIHigh = _IP3_HUEOFINTEREST_HIGHLIMIT_BLUE<<_HUE_SHIFT;			// Set the upper and lower range of the
					unHOILow = _IP3_HUEOFINTEREST_LOWLIMIT_BLUE<<_HUE_SHIFT;			// hue of interest.
					break;
				
					default: // Yellow green.
					unHOIHigh = _IP3_HUEOFINTEREST_HIGHLIMIT_YELLOWGREEN<<_HUE_SHIFT;	// Set the upper and lower range of the
					unHOILow = _IP3_HUEOFINTEREST_LOWLIMIT_YELLOWGREEN<<_HUE_SHIFT;		// hue of interest.
					break;
				}
				OSSetTaskContext(ptrTask, 2, 1);										// Next state = 2, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);			// Next state = 1, timer = 1.
			}				
			break;
			
			case 2: // State 2 - Compare the hue of each pixel with target range and store the binary comparison result in bit 31 of each pixel attribute
			// register.
			
			// --- Column 1 ---
			//PIN_FLAG4_SET;
			for (nYindex = 1; nYindex < gnImageHeight-1; nYindex++)			// Scan through each column, ignore the 1st and
			// last columns, and 1st and last rows.
			{
				if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{												// is valid for access.
					unPixelAttrTemp = gunImgAtt2[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 2.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
				else
				{
					unPixelAttrTemp = gunImgAtt[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 1.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
			}
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-2)									// Is this the last column?
			{
				nXindex = 2;												// Start from Column 2.
				OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				//nState = 3;													// Next state = 3, timer = 1 tick.
				//nTimer = 1;
				break;														// Exit current state.
			}
			
			// --- Column 2 ---
			for (nYindex = 1; nYindex < gnImageHeight-1; nYindex++)			// Scan through each column, ignore the 1st and
			// last columns, and 1st and last rows.
			{
				if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{												// is valid for access.
					unPixelAttrTemp = gunImgAtt2[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 2.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
				else
				{
					unPixelAttrTemp = gunImgAtt[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 1.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
			}
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-2)									// Is this the last column?
			{
				nXindex = 2;												// Start from Column 2.
				OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				//nState = 3;													// Next state = 3, timer = 1 tick.
				//nTimer = 1;
				break;														// Exit current state.
			}
			
			// --- Column 3 ---
			for (nYindex = 1; nYindex < gnImageHeight-1; nYindex++)			// Scan through each column, ignore the 1st and
			// last columns, and 1st and last rows.
			{
				if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{												// is valid for access.
					unPixelAttrTemp = gunImgAtt2[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 2.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
				else
				{
					unPixelAttrTemp = gunImgAtt[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 1.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
			}
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-2)									// Is this the last column?
			{
				nXindex = 2;												// Start from Column 2.
				OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				//nState = 3;													// Next state = 3, timer = 1 tick.
				//nTimer = 1;
				break;														// Exit current state.
			}
			
			// --- Column 4 ---
			for (nYindex = 1; nYindex < gnImageHeight-1; nYindex++)			// Scan through each column, ignore the 1st and
			// last columns, and 1st and last rows.
			{
				if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{												// is valid for access.
					unPixelAttrTemp = gunImgAtt2[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 2.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt2[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
				else
				{
					unPixelAttrTemp = gunImgAtt[nXindex][nYindex];
					unHueTemp = unPixelAttrTemp & _HUE_MASK;					// Extract the hue for current pixel from frame buffer 1.
					if ((unHueTemp < unHOIHigh) && (unHueTemp > unHOILow))		// Check if current pixel is within the upper
					{															// and lower limits of the hue of interest.
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp | 0x80000000;	// Set bit 31 of pixel attribute.  This bit is not used,
						// so we use it to store temporary binary result.
					}
					else
					{
						gunImgAtt[nXindex][nYindex] = unPixelAttrTemp & 0x7FFFFFFF;	// Clear bit 31 of pixel attribute.
					}
				}
			}
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-2)									// Is this the last column?
			{
				nXindex = 2;												// Start from Column 2.
				OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				//nState = 3;													// Next state = 3, timer = 1 tick.
				//nTimer = 1;
			}
			else
			{
				OSSetTaskContext(ptrTask, 2, 1);							// Next state = 2, timer = 1.
				//nState = 2;													// Next state = 2, timer = 1 tick.
				//nTimer = 1;
			}
			break;

			case 3: // State 3 - Read the pixel attribute register of each pixel and it's 4 adjacent pixels (see below), extract bit31 for
			// each and filter for interior pixel.  Only interior marked pixels (a pixel matching the target hue, surrounded by 4
			// other pixels which also match the target hue) are selected.  The row and column histogram registers are then updated
			// to find the row and column with the most number of matched interior pixels.  The result of the filtering is also stored
			// in the result buffer gunIPResult[][] for remote monitoring if necessary.
			// Naming convention:
			// Let P denotes current pixel under analysis.  The adjacent pixels are numbered as follows:
			//                    \
			// |-------------------- 'X'
			// |                  /
			// |  0 1 2
			// |  7 P 3   P=Pixel of interest
			//\|/ 6 5 4
			// 'Y'

			// --- Column 1 ---
			nHueCounterCol = 0;												// Reset counter to keep track of no. of valid pixels along the column.
			for (nYindex = 2; nYindex < gnImageHeight-2; nYindex++)			// Scan through each column, ignore the 1st two
																			// and last two columns.
			{
				unResult = 0;												// Reset result value.
				if (gnValidFrameBuffer == 1)								// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{															// is valid for access.
					unPixel = gunImgAtt2[nXindex][nYindex] & 0x80000000;	// Get the status of bit31 for current pixel and 4 adjacent pixels.
					unPixel1 = gunImgAtt2[nXindex][nYindex - 1] & 0x80000000;
					unPixel3 = gunImgAtt2[nXindex + 1][nYindex] & 0x80000000;
					unPixel5 = gunImgAtt2[nXindex][nYindex + 1] & 0x80000000;
					unPixel7 = gunImgAtt2[nXindex - 1][nYindex] & 0x80000000;
					nCounter = 0;											// Reset counter.
					if (unPixel > 0)										// Only proceed if current pixel is 'active' or marked.
					{
						//nCounter = 1;
						if (unPixel == unPixel1)							// Compare current pixel with 4 adjacent pixels.
						{
							nCounter++;
						}
						if (unPixel == unPixel3)
						{
							nCounter++;
						}
						if (unPixel == unPixel5)
						{
							nCounter++;
						}
						if (unPixel == unPixel7)
						{
							nCounter++;
						}
					}
				}
				else
				{
					unPixel = gunImgAtt[nXindex][nYindex] & 0x80000000;		// Get the status of bit31 for current pixel and 4 adjacent pixels.
					unPixel1 = gunImgAtt[nXindex][nYindex - 1] & 0x80000000;
					unPixel3 = gunImgAtt[nXindex + 1][nYindex] & 0x80000000;
					unPixel5 = gunImgAtt[nXindex][nYindex + 1] & 0x80000000;
					unPixel7 = gunImgAtt[nXindex - 1][nYindex] & 0x80000000;
					nCounter = 0;											// Reset counter.
					if (unPixel > 0)										// Only proceed if current pixel is 'active' or marked.
					{
						//nCounter = 1;
						if (unPixel == unPixel1)							// Compare current pixel with 4 adjacent pixels.
						{
							nCounter++;
						}
						if (unPixel == unPixel3)
						{
							nCounter++;
						}
						if (unPixel == unPixel5)
						{
							nCounter++;
						}
						if (unPixel == unPixel7)
						{
							nCounter++;
						}
					}
				}
				
				if (nCounter > 3)				// nCounter is 4 if the current marked pixel is surrounded by 4 adjacent
				{								// marked pixels.
					unResult = 255;				// Result to be updated in result buffer.
					nHOIHistoRow[nYindex]++;	// Update row histogram.
					nHueCounterCol++;			// Update valid pixel in column counter.
				}
				else if (nCounter > 0)			// This means the current marked pixel meets the target hue criteria, but
				{								// is not surrounded by 4 adjacent marked pixels.  We use dimmer
					unResult = 70;				// luminance to highlight the pixel in the result buffer.
				}
				// Store the result of computation into the result buffer.
				SetIPResultBuffer(nXindex, nYindex, unResult);
			}
			nHOIHistoCol[nXindex] = nHueCounterCol; 						// Update column histogram to maximum count value.
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-3)									// Is this the last column?
			{
				OSSetTaskContext(ptrTask, 4, 1);							// Next state = 4, timer = 1.
				//nState = 4;													// Next state = 4, timer = 1 tick.
				//nTimer = 1;
				break;														// Exit current state.
			}
			
			// --- Column 2 ---
			nHueCounterCol = 0;												// Reset counter to keep track of no. of valid pixels along the column.
			for (nYindex = 2; nYindex < gnImageHeight-2; nYindex++)			// Scan through each column, ignore the 1st two
																			// and last two columns.
			{
				unResult = 0;												// Reset result value.
				if (gnValidFrameBuffer == 1)								// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
				{															// is valid for access.
					unPixel = gunImgAtt2[nXindex][nYindex] & 0x80000000;	// Get the status of bit31 for current pixel and 4 adjacent pixels.
					unPixel1 = gunImgAtt2[nXindex][nYindex - 1] & 0x80000000;
					unPixel3 = gunImgAtt2[nXindex + 1][nYindex] & 0x80000000;
					unPixel5 = gunImgAtt2[nXindex][nYindex + 1] & 0x80000000;
					unPixel7 = gunImgAtt2[nXindex - 1][nYindex] & 0x80000000;
					nCounter = 0;											// Reset counter.
					if (unPixel > 0)										// Only proceed if current pixel is 'active' or marked.
					{
						//nCounter = 1;
						if (unPixel == unPixel1)							// Compare current pixel with 4 adjacent pixels.
						{
							nCounter++;
						}
						if (unPixel == unPixel3)
						{
							nCounter++;
						}
						if (unPixel == unPixel5)
						{
							nCounter++;
						}
						if (unPixel == unPixel7)
						{
							nCounter++;
						}
					}
				}
				else
				{
					unPixel = gunImgAtt[nXindex][nYindex] & 0x80000000;		// Get the status of bit31 for current pixel and 4 adjacent pixels.
					unPixel1 = gunImgAtt[nXindex][nYindex - 1] & 0x80000000;
					unPixel3 = gunImgAtt[nXindex + 1][nYindex] & 0x80000000;
					unPixel5 = gunImgAtt[nXindex][nYindex + 1] & 0x80000000;
					unPixel7 = gunImgAtt[nXindex - 1][nYindex] & 0x80000000;
					nCounter = 0;											// Reset counter.
					if (unPixel > 0)										// Only proceed if current pixel is 'active' or marked.
					{
						//nCounter = 1;
						if (unPixel == unPixel1)							// Compare current pixel with 4 adjacent pixels.
						{
							nCounter++;
						}
						if (unPixel == unPixel3)
						{
							nCounter++;
						}
						if (unPixel == unPixel5)
						{
							nCounter++;
						}
						if (unPixel == unPixel7)
						{
							nCounter++;
						}
					}
				}
				
				if (nCounter > 3)				// nCounter is 4 if the current marked pixel is surrounded by 4 adjacent
				// marked pixels.
				{
					unResult = 255;				// Result to be updated in result buffer.
					nHOIHistoRow[nYindex]++;	// Update row histogram.
					nHueCounterCol++;			// Update valid pixel in column counter.
				}
				else if (nCounter > 0)			// This means the current marked pixel meets the target hue criteria, but
				{								// is not surrounded by 4 adjacent marked pixels.  We use dimmer
					unResult = 70;				// luminance to highlight the pixel in the result buffer.
				}				
				// Store the result of computation into the result buffer.
				SetIPResultBuffer(nXindex, nYindex, unResult);
			}
			nHOIHistoCol[nXindex] = nHueCounterCol; 						// Update column histogram to maximum count value.
			nXindex++;														// Next column.
			if (nXindex == gnImageWidth-3)									// Is this the last column?
			{
				OSSetTaskContext(ptrTask, 4, 1);							// Next state = 4, timer = 1.
				//nState = 4;													// Next state = 4, timer = 1 tick.
				//nTimer = 1;
				break;														// Exit current state.
			}
			
			OSSetTaskContext(ptrTask, 3, 1);								// Next state = 3, timer = 1.
			//nState = 3;														// Next state = 3, timer = 1 tick.
			//nTimer = 1;
			break;

			case 4: // State 4 - Check for the column with large histogram value.  Add marker 1 to the external display to highlight the result.
			//PIN_FLAG4_SET;
			nMaxValueCol = _IP3_VALID_PIXEL_THRESHOLD;							// Initialize the maximum histogram value and column.
			nMaxCol = -1;														// Note: The initial value serves as a threshold.  Only when
			// the values in the HOI histogram above this threshold will the
			// values in the histogram array be considered.  This threshold can
			// be set in following manner:
			// Say the number of pixels in a column is 120 pixels.  We could
			// select a threshold of 5% of the column pixels, e.g. 6 pixels.
			// This rule-of-thumb allow sufficient guard against noise.
			
			for (nIndex = 1; nIndex < gnImageWidth-2; nIndex++)					// Scan through each column histogram, and compare with the maximum.
			// histogram value.  Ignore Column 0 and last column.
			{
				if (nHOIHistoCol[nIndex] > nMaxValueCol)						// If current histogram value is larger than nMaxValue,
				{																// update nMaxValue and the corresponding index.
					nMaxValueCol = nHOIHistoCol[nIndex];
					nMaxCol = nIndex;
				}
			}
			nMaxValueRow = _IP3_VALID_PIXEL_THRESHOLD;
			nMaxRow = -1;
			for (nIndex = 1; nIndex < gnImageHeight-2; nIndex++)				// Scan through each row histogram, and compare with the maximum.
			// histogram value.  Ignore Column 0 and last column.
			{
				if (nHOIHistoRow[nIndex] > nMaxValueRow)						// If current histogram value is larger than nMaxValue,
				{																// update nMaxValue and the corresponding index.
					nMaxValueRow = nHOIHistoRow[nIndex];
					nMaxRow = nIndex;
				}
			}
			
			if ((nMaxCol > 0) && (nMaxRow > 0))									// If both valid column or row value exists.
			{
				if (nMaxValueRow > nMaxValueCol)
				{
					nTemp = nMaxValueRow;
				}
				else
				{
					nTemp = nMaxValueCol;
				}
				gnCameraLED = 4;												// Set camera LED to moderate, to signal that object of interest is detected
				// in current frame.
				gobjRec1.nHeight = nTemp;										// Enable a square marker to be displayed in remote monitor
				gobjRec1.nWidth = nTemp;										// software.
				gobjRec1.nX = nMaxCol;											// Set the location of the marker. 
				gobjRec1.nY = nMaxRow;
			}
			else
			{
				gnCameraLED = 1;												// Set camera LED to low, to signal that no object of interest (with matching color)
																				// is detected.
				gobjRec1.nHeight = 0 ;											// Disable square marker to be displayed in remote monitor
				gobjRec1.nWidth = 0 ;											// software.
				gobjRec1.nX = -500;												// Set the location of the marker. When no valid color object is detected,
				gobjRec1.nY = -500;												// the (x,y) coordinate is set to a large negative value.
			}
			
			switch (gunIPA3_Argument)										    // Check for the hue to identify and set marker color accordingly.
			{
				case 1: // Red.
				gobjRec1.nColor = 1;
				break;
				
				case 2: // Green.
				gobjRec1.nColor = 2;
				break;
				
				case 3: // Blue.
				gobjRec1.nColor = 3;
				break;
				
				default: // Yellow green.
				gobjRec1.nColor = 4;
				break;
			}
			
			gobjRec2.nHeight = 3 ;												// Enable a square marker to be displayed in remote monitor
			gobjRec2.nWidth = 3 ;												// software.  This marker marks the point where the hue will
			gobjRec2.nX = 79;													// be transmitted to the remote monitor for debugging purpose.
			gobjRec2.nY = 59;
			gobjRec2.nColor = 2;
			gunHue = (gunImgAtt[80][60] & _HUE_MASK)>>(_HUE_SHIFT+1);			// Send the hue/2 at the center pixel, for debugging purpose.
			OSSetTaskContext(ptrTask, 5, 1);									// Next state = 5, timer = 1.
			//nState = 5;															// Next state = 5, timer = 1 tick.
			//nTimer = 1;
			break;
			
			
			case 5: // State 5 - Transmit status to external controller, part 1 (transmit first 2 bytes).
			// NOTE: 28 Dec 2016.  To prevent overflow error on the remote controller (as it does not use DMA on the UART),
			// we avoid sending all 4 bytes one shot, but split the data packet into two packets or 2 bytes each.  A 1 msec
			// delay is inserted between each two bytes packet.  As the algorithm in the remote controller improves in future
			// this artificial restriction can be removed.
			gbytTXbuffer2[0] = 3;		// Load data, process ID.
			if (nMaxValueRow > nMaxValueCol)
			{
				gbytTXbuffer2[1] = nMaxValueRow;			// The max value is an indication of the object size.
			}
			else
			{
				gbytTXbuffer2[1] = nMaxValueCol;			//
			}
			//nState = 6;										// Next state = 6, timer = 1 msec.
			//nTimer = 1*__NUM_SYSTEMTICK_MSEC;
			OSSetTaskContext(ptrTask, 6, 1*__NUM_SYSTEMTICK_MSEC);		// Next state = 6, timer = 1 msec.
			break;

			case 6: // State 6 - Transmit status to external controller, part 2 (transmit last 2 bytes).
			if ((nMaxCol > 0) && (nMaxRow > 0))	// Check if the coordinate is valid, e.g. object matching HOI is detected.
			{
				gbytTXbuffer2[2] = nMaxCol;
				gbytTXbuffer2[3] = nMaxRow;
			}
			else
			{
				gbytTXbuffer2[2] = 255;			// Indicate invalid coordinate.  Basically 0xFF = -1 in 8-bits 2's complement.
				gbytTXbuffer2[3] = 255;
			}
			//nState = 7;							// Next state = 7, timer = 1 msec.
			//nTimer = 1*__NUM_SYSTEMTICK_MSEC;
			OSSetTaskContext(ptrTask, 7, 1*__NUM_SYSTEMTICK_MSEC);		// Next state = 7, timer = 1 msec.
			break;
			
			case 7: // State 7 - End, clear busy flag to let other processes know that current algorithm completes.
			//gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_IDLE;
			//PIN_FLAG4_CLEAR;						// Clear indicator flag.
			//nState = 1;								// Next state = 1, timer = 1 tick.
			//nTimer = 1;
			OSSetTaskContext(ptrTask, 1, 1);		// Next state = 1, timer = 1 tick.
			break;
			
			default:
			OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
			//nState = 0;								// Next state = 0, timer = 1 tick.
			//nTimer = 1;
			break;
		}
	}
}

