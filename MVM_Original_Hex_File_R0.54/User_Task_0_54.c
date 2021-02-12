//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT)
//
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: User_Task.c
// Author(s)		: Fabian Kung
// Last modified	: 11 Feb 2021
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.0.1

#include "osmain.h"
#include "Driver_UART2_V100.h"
#include "Driver_USART0_V100.h"
#include "Driver_TCM8230.h"

// NOTE: Public function prototypes are declared in the corresponding *.h file.

//
//
// --- PUBLIC VARIABLES ---
//
typedef struct StructMARKER
{
	unsigned int nAttrib;				// Attribute of marker
	unsigned int nX;					// X coordinate of marker.
	unsigned int nY;					// y coordinate of marker.
} MARKER;

MARKER	objMarker1;
MARKER	objMarker2;
MARKER	objMarker3;

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

MARKER_RECTANGLE	gobjRec1;			// Marker objects.
MARKER_RECTANGLE	gobjRec2;
MARKER_RECTANGLE	gobjRec3;


unsigned int	gunPtoALuminance;		// Peak-to-average luminance value of each frame.
unsigned int	gunMaxLuminance;		// Maximum luminance in curret frame, for debug purpose.
unsigned int	gunMinLuminance;		// Minimum luminance in current frame, for debug purpose.
unsigned int	gunHue;					// For debug purpose.

int		gnSendSecondaryInfo = 0;		// Set to 1 (in Image Processing Tasks) to transmit secondary info such as marker location, 
										// size attribute and string to remote display.  Else set to 0.  If this is set 
										// to 1, after the transmission of a line of pixel data, the secondary info
										// packet will be send, and this will be clear to 0 by Proce_MessageLoop_StreamImage().
										// In present implementation the Proce_MessageLoop_StreamImage() will automatically 
										// set this to 1 after transmitting a frame to send secondary info automatically.

										// Assuming the frame rate is 20 frame-per-second (fps), then the duration for each
										// frame interval will be 1/20 = 50 msec.  These frame interval will be labeled interval
										// 1, interval 2, interval 3 and up to M intervals, then repeat again.  At each frame
										// interval m, the image processing algorithm (IPA) to execute depends on the value store
										// in the global variable gunRunIPAIntervalm, where m = 1, 2, 3 ... M-1, M.
										// Example:
										//	For instance M = 2, and we have 
										//	gunRunIPAInterval1 = 2 and gunRunIPAInterval2 = 3.
										//  
										// IPA2 and IPA3 will be executed alternately for each new image frame received in the 
										// frame buffers.  The response time for each IPA will be 100 msec.  Now suppose there
										// is a situation where we do not need to use IPA3, only IPA2 and will need faster 
										// response time, then we can set 
										// gunRunIPAInterval1 = 2 and gunRunIPAInterval2 = 2.
										//  
										// The second scenario is like a complex organism needs to focus on a task (instead of
										// multi-tasking).  Hence this method allows us to mix-and-match any two IPA that are
										// at our disposal to produce 'normal' multi-tasking and 'focused' state of operation.
										//
unsigned int	gunRunIPAInterval1 = 0x0;	// The value store in this variable represents the image processing algorithm (IPA)
										// to execute during time interval 1. A value of 0 means no IPA will be executed.
unsigned int	gunRunIPAInterval2 = 0x0;	// The value store in this variable represents the image processing algorithm (IPA)
										// to execute during time interval 2. A value of 0 means no IPA will be executed.	
										
unsigned int	gunIPA1_Argument = 0;	// Variable to store optional argument for each image processing algorithm (IPA).	
unsigned int	gunIPA2_Argument = 0;
unsigned int	gunIPA3_Argument = 0;			
																	
int		gnImageProcessingArg = 0;		// Argument for image processing algorithm.
int		gnImageProcessingAlgorithmBusy = 0;	// 0 - The current image processing algorithm completes.
											// otherwise - The current image processing algorithm is still running.

#define		_IMAGEPROCESSINGALGORITHM_BUSY	1
#define		_IMAGEPROCESSINGALGORITHM_IDLE	0		
										
unsigned int	gunIPResult[_IMAGE_HRESOLUTION/4][_IMAGE_VRESOLUTION];

// --- Local function prototype ---

void	SetIPResultBuffer(int , int , unsigned int);	// Function to update the bytes in gunIPResult.
void	ImageProcessingAlgorithm1(void);
void	ImageProcessingAlgorithm2(void);
void	ImageProcessingAlgorithm3(void);

// --- Local Constants ---

#define     _DEVICE_RESET               0x00
#define     _RES_PARAM                  0x01
#define     _SET_PARAM                  0x02

// Note: 18 Sep 2019 - Certain HC-05 module is active low while others are active high.  There only way to know which is which is to 
// power up the HC-05 module and check the voltage level on the 'EN' pin.  If it is high after the module initialized, then it is 
// of the active low type.  Else the module EN pin is active high type.
#define PIN_HC_05_RESET_CLEAR   PIOD->PIO_ODSR |= PIO_ODSR_P24                   // Pin PD24 = Reset pin for HC-05 Bluetooth module.
#define PIN_HC_05_RESET_SET		PIOD->PIO_ODSR &= ~PIO_ODSR_P24					 // Active low.

//#define PIN_HC_05_RESET_SET   PIOD->PIO_ODSR |= PIO_ODSR_P24                   // Pin PD24 = Reset pin for HC-05 bluetooth module.
//#define PIN_HC_05_RESET_CLEAR		PIOD->PIO_ODSR &= ~PIO_ODSR_P24					 // Active high.

#define PIN_FLAG3_SET			PIOA->PIO_ODSR |= PIO_ODSR_P7;					// Set flag 3.
#define PIN_FLAG3_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P7;					// Clear flag 3.
#define PIN_FLAG4_SET			PIOA->PIO_ODSR |= PIO_ODSR_P8;					// Set flag 4.
#define PIN_FLAG4_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P8;					// Clear flag 4.

/// Author			: Fabian Kung
/// Last modified	: 7 Sep 2018
/// Description		:
/// This function is used to set the byte in gunIPResult[][] array.  
/// Arguments: nx - X-coordinate in the array.
///            ny - Y-coordinate in the array.
///            unValue - 8-bits luminance value.
/// Return:	   None. 

void	SetIPResultBuffer(int nx, int ny, unsigned int unResult)
{
	int	nTemp;
					
	nTemp = nx / 4;		// Shrink the column index by 4, because each unsigned integer contains 4 bytes.
	switch (nx % 4)		// The unsigned integer format is 32-bits in ARM Cortex-M.  However each pixel
						// computation result is 8-bits, thus each unsigned integer word can store results
						// for 4 pixels.
						// Bit 0-7:		pixel 1
						// Bit 8-15:	pixel 2
						// Bit 16-23:	pixel 3
						// Bit 24-31:	pixel 4
	{
		case 0:	// nIndex % 4 = 0	(nIndex = 0,4,8 etc)
			gunIPResult[nTemp][ny] = (gunIPResult[nTemp][ny] & 0xFFFFFF00) + unResult;			// Set the lowest 8-bits of the 32-bits word.
		break;
						
		case 1: // nIndex % 4 = 1	(nIndex = 1,5,9 etc)
			gunIPResult[nTemp][ny] = (gunIPResult[nTemp][ny] & 0xFFFF00FF) + (unResult<<8);		// Set bits 8-15 of the 32-bits word.
		break;
						
		case 2: // nIndex % 4 = 2	(nIndex = 2,6,10 etc)
			gunIPResult[nTemp][ny] = (gunIPResult[nTemp][ny] & 0xFF00FFFF) + (unResult<<16);	// Set bits 16-23 of the 32-bits word.
		break;
				
		default: // nIndex % 4 = 3	(nIndex = 3,7,11 etc)
			gunIPResult[nTemp][ny] = (gunIPResult[nTemp][ny] & 0x00FFFFFF) + (unResult<<24);	// Set bits 24-31 of the 32-bits word.
		break;
	}
}

///
/// Function name	: Proce_MessageLoop_StreamImage
///
/// Author			: Fabian Kung
///
/// Last modified	: 16 Sep 2019
///
/// Code Version	: 1.06
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			: Pin PD24 = Reset pin for HC-05 BlueTooth module.
///
/// MODULES			: UART0 (Internal), XDMA (Internal), USART0 (Internal).
///
/// RTOS			: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    :

#ifdef __OS_VER			// Check RTOS version compatibility.
#if __OS_VER < 1
#error "Proce_MessageLoop_StreamImage: Incompatible OS version"
#endif
#else
#error "Proce_MessageLoop_StreamImage: An RTOS is required with this function"
#endif

///
/// Description	:
/// This process performs two (2) key tasks:
/// 1. Message clearing loop for instructions from external controller via USART0 port.
/// 2. Message clearing loop and stream video image captured by the camera via UART0 port.
///
/// --- (1) Message clearing from External Controller ---
/// Basically this process waits for 1-byte command from external controller. The commands are as
/// follows:
/// Command:	Reply:  	Action:
/// [CMD]		None		The binary value of CMD determines the image processing algorithm (IPA)
///							to run. The upper nibble represents IPA to run, and the lower nibble
///                         represents the optional argument for the IPA, thus at present this method
///                         supports up to 15 IPAs (discounting the value 0). 
///
/// --- (2) Stream video image ---
/// Stream the image captured by camera to remote display via UART port.  Note that the UART port
/// can be connected to a wireless module such as the HC-05 Blue tooth module.
/// Here our main remote display is a computer running Windows OS.
///
/// The remote display initiates the transfer by sending a single byte command to the processor.
/// 1. If the command is 'L', 'R', 'G', 'B', then luminance data will be stream to the remote processor.
///    The characters determine how the luminance is computed from the RGB components of each pixel.
///    See the codes for the camera driver for details.
/// 2. If the command is 'D', then luminance gradient data will be streamed to the remote processor.
/// 3. If the command is 'H', then hue data will be streamed to the remote processor.  Here we
///    compress the Hue range from 0-360 to 0-90 (e.g. divide by 4) so that it will fit into 7 bits.
///
/// The image data is send to the remote display line-by-line, using a simple RLE (Run-Length
/// Encoding) compression format. The data format:
///
/// Byte0: 0xFF (Indicate start-of-line)
/// Byte1: Line number, 0-253, indicate the line number in the bitmap image.
///        If Byte1 = 254, it indicate the subsequent bytes are secondary info such
///        as ROI location and size and any other info the user wish to transmit to the host.
///        At present auxiliary info is only 10 bytes.
/// Byte2: The length of the data payload, excluding Byte0-2.
/// Byte3-ByteN: Data payload.
/// The data payload only accepts 7 bits value, from bit0-bit6.  Bit7 is used to indicate
/// repetition in the RLE algorithm, the example below elaborate further.
///
/// Example 1 - One line of pixel data.
/// Consider the following byte stream:
/// [0xFF][3][7][90][80][70][0x83][60][0x82][120]
///
/// Byte0 = 255 of 0xFF, indicate start of line.
/// Byte1 = 3 is the line no. of the current pixel data in a 2D image.
/// Byte2 = 7 indicates that there are 7 data bytes in this data packet.
/// Byte3 = 90 represent the 1st pixel data in line 3.  This value can represent the
///         luminance value of other user define data.
/// Byte4 = 80 another pixel data.
/// Byte5 = 70 another pixel data.
/// Byte6 = 0x83 = 0b10000000 + 3.  This indicates the last byte, e.g. Byte5=70 is to
///         be repeated 3 times, thus there are a total of 4 pixels with value of 70
///         after pixel with value of 80.
/// Byte7 = 60 another pixel data.
/// Byte8 = 0x82 = 0b10000000 + 2.  The pixel with value of 60 is to be repeated 2 times.
/// Byte9 = 120, another pixel.
///
/// The complete line of pixels is as follows:
/// Line3: [90][80][70][70][70][70][60][60][60][120]
///
/// The maximum no. of repetition: Currently the maximum no. of repetition is 63.  If
/// there are 64 consecutive pixels with values of 70, we would write [70][0x80 + 0x3F]
/// or [70][0x10111111].  The '0' at bit 6 is to prevention a repetition code of 0xFF, which
/// can be confused with start-of-line value.
///
/// Example 2 - Secondary information
/// Consider the following byte stream:
/// [0xFF][255][5][40][30][60][60][1]
/// The length of the payload is 5 bytes.
/// This instructs the remote monitor to highlight a region of 40x30 pixels, starting at position
/// (x,y) = (60,60) with color code = 1.
///
/// 25 Dec 2015: What I discovered is that there is a latency in Windows 8.1 in terms.
/// of processing the data from UART-to-USB converter.  Initially I send only 1 line of
/// pixel data per packet.  My observation using oscilloscope is that after the packet is
/// received, there is a delay on the order of 10-20 msec before the application is notified.
/// It seems this delay is not dependent of the number of bytes in the packet.  Example if we
/// transmit only 2 bytes, there is still a latency of this amount.  Thus it is more efficient
/// to transmit more bytes per packet.  The actual duration to display the data in the
/// application window is on the order of 1-3 msec.  In order to improve efficiency, we can
/// transmit 2 or more lines of pixel data in one packet of data.


void Proce_MessageLoop_StreamImage(TASK_ATTRIBUTE *ptrTask)
{
	static unsigned char bytData;
	static int nLineCounter;
	int nXposCounter;
	int nCurrentPixelData;
	int nRefPixelData;
	int nRepetition;
	int nTemp, nTemp2;
	int nIndex;
	static nIPAInterval;
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization.
			objMarker1.nAttrib = 0;			// Disable plotting of all markers in the graphic window of Remote host.
			objMarker2.nAttrib = 0;
			objMarker3.nAttrib = 0;
			PIN_HC_05_RESET_CLEAR;          // De-assert reset pin for BlueTooth module.
			PIN_LED2_CLEAR;					// Turn off indicator LED2.
			nLineCounter = 0;
			nIPAInterval = 0;
			OSSetTaskContext(ptrTask, 10, 1200*__NUM_SYSTEMTICK_MSEC);     // Next state = 10, timer = 1200 msec.
			break;

			case 1: // State 1 - a) Message clearing for USART0 and UART2.
					// For USART0, each command byte has the following format:
					// Upper nibble: 0-15, indicating the image processing algorithm (IPA) ID to run. Zero
					// means no IPA is running.  
					// Lower nibble: 0-15, optional argument for the IPA.  For instance for IPA to recognize
					// color objects, the lower nibble value 0-15 represent 16 different hues.
					//
					// For UART2: Wait for start signal from remote host.
					// The start signal is the character 'L' (for luminance data using RGB)
					// 'R' (for luminance data using R only)
					// 'G' (for luminance data using G only)
					// 'B' (for luminance data using B only)
					// 'G' (for gradient data).
					// 'H' (for hue data)
					// 'P' (for image processing buffer result)
			
			// --- Message clearing for USART0 ---			
			// Only process the top two bytes received in the receive buffer, then discard the rest.
			if (gbytRXbufptr2 > 0)						// Check if USART0 receive at least 1 byte of data.
			{
				if (gSCIstatus2.bRXOVF == 0)			// Make sure no overflow error.
				{
					while(gbytRXbufptr2 > 0)			// While the is data in the receive buffer.
					{
						gbytRXbufptr2--; 				// Decrement pointer.
						nTemp = gbytRXbuffer2[gbytRXbufptr2] >> 4;			// Get the top most byte of the receive buffer,
																			// and mask out the lower nibble.  
						nTemp2 = gbytRXbuffer2[gbytRXbufptr2] & 0x0F;		// Get lower nibble, the argument for the IPA.
						nIPAInterval++;
						if (nIPAInterval == 2)
						{
							nIPAInterval = 0;
							gunRunIPAInterval2 = nTemp;						// Set the IPA ID for interval 2.
						}
						else
						{
							gunRunIPAInterval1 = nTemp;						// Set the IPA ID for interval 1.
						}
						
						switch (nTemp)										// Assign the argument to the correct IPA register.
						{
							case 1:
								gunIPA1_Argument = nTemp2;
								break;
							case 2:
								gunIPA2_Argument = nTemp2;
								break;							
							case 3:
								gunIPA3_Argument = nTemp2;
								break;														
						}						
					}
				}
				else
				{
					gSCIstatus2.bRXOVF = 0; 	// Reset overflow error flag.
					gbytRXbufptr2 = 0; 			// Clear pointer.
				}
				gSCIstatus2.bRXRDY = 0;			// Reset valid data flag.
				PIN_LED2_CLEAR;					// Turn off indicator LED2.
			}
			
			// --- Message clearing and stream video image for UART2 ---
			if (gSCIstatus.bRXRDY == 1)	// Check if UART receive any data.
			{
				if (gSCIstatus.bRXOVF == 0) // Make sure no overflow error.
				{
					bytData = gbytRXbuffer[0];	// Get 1 byte and ignore all others.
					switch (bytData)
					{
						case 'L':								// Send luminance data computed from RGB components.  The camera driver
																// will use all R,G and B components to compute the luminance.	
							gnLuminanceMode = 0;	
							PIN_FLAG3_SET;		
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
						break;
						
						case 'D':								// Luminance gradient data.
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
						break;
						
						case 'H':								// Hue data.
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
						break;
						
						case 'R':								// Send luminance data computed from R component.  The camera driver 
							gnLuminanceMode = 1;				// pre-processing will compute the luminance using R component.
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.						
						break;
						
						case 'G':								// Send luminance data computed from G component. The camera driver
							gnLuminanceMode = 2;				// pre-processing will compute the luminance using G component.
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.						
						break;
						
						case 'B':								// Send luminance data computed from B component. The camera driver
							gnLuminanceMode = 3;				// pre-processing will compute the luminance using B component.	
							OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.						
						break;
						
						case 'P':								// Send the result from Image Processing Result Buffer (IPRB).
																// No compression of data.
							OSSetTaskContext(ptrTask, 3, 1);    // Next state = 3, timer = 1.
						break;
						
						default:
							OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
						break;
					}
				}
				else
				{
					gSCIstatus.bRXOVF = 0; 	// Reset overflow error flag.
					OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
				}
				gSCIstatus.bRXRDY = 0;	// Reset valid data flag.
				gbytRXbufptr = 0; 		// Reset pointer.
				PIN_LED2_CLEAR;			// Turn off indicator LED2.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
			}

			break;
			
			case 2: // State 2 - Send a line of pixel data to remote host, with RLE data compression.
			
			if (gSCIstatus.bTXRDY == 0)					// Check if  UART port is busy.
			{
				gbytTXbuffer[0] = 0xFF;					// Start of line code.
				gbytTXbuffer[1] = nLineCounter;			// Line number.
				// gbytTXbuffer[2] stores the payload length.
				// --- Implement simple RLE algorithm ---
				nRepetition = 0;						// Initialize repetition counter.
				nXposCounter = 1;						// Initialize x-position along a line of pixels.
				// Initialize the first data byte.
				// Get 1st byte/pixel.

				if (bytData != 'D') 					// Read pixel data based on format selection.
				{
					if (bytData == 'P')
					{
						nCurrentPixelData = (gunIPResult[0][nLineCounter]) & 0x000000FF;  // Get the lowest 8-bits from the 1st word.
					}
					else if (bytData != 'H')					// 'L', 'R', 'G', 'B', send luminance data.
					{					
						nCurrentPixelData = gunImgAtt[0][nLineCounter] & _LUMINANCE_MASK; // Get pixel luminance data from frame buffer 1.
						nCurrentPixelData = nCurrentPixelData>>1;		// Note: Only 7-bits data is allowed
					}
					else                                // 'H', send scaled hue info.  Here we rescale the hue (0-360)
					{									// to between 0-90, e.g. Hue/4.
						nTemp = gunImgAtt[0][nLineCounter] & _HUE_MASK;
						nCurrentPixelData = nTemp >> (_HUE_SHIFT + 2);
					}
				}
				else     // bytData == 'D'.
				{
					nCurrentPixelData = gunImgAtt[0][nLineCounter] >> (_GRAD_SHIFT + 1); // Get pixel gradient data and divide
					// by 2, as gradient value can hit 255.
					nCurrentPixelData = 0x000000FF & nCurrentPixelData; // Mask out all except lower 8 bits.
				}
				
				nRefPixelData = nCurrentPixelData;		// Setup the reference value.
				gbytTXbuffer[3] = nCurrentPixelData;	// First byte of the data payload.
				
				// Get subsequent bytes/pixels in the line of pixels data.
				for (nIndex = 1; nIndex < gnImageWidth; nIndex++)
				{

					if (bytData == 'P')
					{
						nTemp = nIndex / 4;				// Shrink the column index by 4, because each unsigned integer contains 4 bytes.
						switch (nIndex % 4)				// The unsigned integer format is 32-bits in ARM Cortex-M.  However each pixel
														// computation result is 8-bits, thus each unsigned integer word can store results
														// for 4 pixels.  
														// Bit 0-7:		pixel 1
														// Bit 8-15:	pixel 2
														// Bit 16-23:	pixel 3
														// Bit 24-31:	pixel 4	
						{
							case 0:	// nIndex % 4 = 0	(nIndex = 0,4,8 etc)
								nCurrentPixelData = gunIPResult[nTemp][nLineCounter] & 0x000000FF;	// Get the lowest 8-bits from 32-bits word.
							break;
							
							case 1: // nIndex % 4 = 1	(nIndex = 1,5,9 etc)
								nCurrentPixelData = (gunIPResult[nTemp][nLineCounter] & 0x0000FF00)>>8;	// Get the 2nd 8-bits from 32-bits word.
							break;
							
							case 2: // nIndex % 4 = 2	(nIndex = 2,6,10 etc)
								nCurrentPixelData = (gunIPResult[nTemp][nLineCounter] & 0x00FF0000)>>16;	// Get the 3rd 8-bits from 32-bits word.
							break;
							
							default: // nIndex % 4 = 3	(nIndex = 3,7,11 etc)
								nCurrentPixelData = gunIPResult[nTemp][nLineCounter]>>24;			// Get the highest 8-bits from 32-bits word.
							break;
						}
						nCurrentPixelData = nCurrentPixelData>>1;		// Note: Only 7-bits data is allowed.
					}
					else if (bytData == 'H')							// 'H', send scaled hue info.  Here we rescale the hue (0-360)
					{													// to between 0-90, e.g. Hue/4.
						nTemp = gunImgAtt[nIndex][nLineCounter] & _HUE_MASK;
						nCurrentPixelData = nTemp >> (_HUE_SHIFT + 2);
					}
					else if (bytData == 'D')
					{
						nCurrentPixelData = gunImgAtt[nIndex][nLineCounter] >> (_GRAD_SHIFT + 1);	// Get pixel gradient data and divide
																									// by 2, as the gradient value can hit 255.
						nCurrentPixelData = 0x0000007F & nCurrentPixelData; // Mask out all except lower 7 bits.						
					}
					else                                
					{													// 'L', 'R', 'G', 'B', send luminance data.
						nCurrentPixelData = gunImgAtt[nIndex][nLineCounter] & _LUMINANCE_MASK; // Get pixel luminance data from frame buffer 1.				
					}	
					
					if (nCurrentPixelData == nRefPixelData) // Current and previous pixels share similar value.
					{
						nRepetition++;
						if (nIndex == gnImageWidth-1)	   // Check for last pixel in line.
						{
							gbytTXbuffer[nXposCounter+3] = 128 + nRepetition; // Set bit7 and add the repetition count.
							nXposCounter++;
						}
						else if (nRepetition == 64)			// The maximum no. of repetition allowed is 63, see explanation
						{									// in 'Description'.
							gbytTXbuffer[nXposCounter+3] = 128 + 63; // Set bit7 and add the repetition count.
							nXposCounter++;					// Point to next byte in the array.
							nRepetition = 0;				// Reset repetition counter.
							gbytTXbuffer[nXposCounter+3] = nCurrentPixelData;
							nXposCounter++;					// Point to next byte in the array.
						}
					}
					else									// Current and previous pixel do not share similar value.
					{
						nRefPixelData = nCurrentPixelData;	// Update reference pixel value.
						if (nRepetition > 0)				// Check if we have multiple pixels of similar value.
						{
							gbytTXbuffer[nXposCounter+3] = 128 + nRepetition; // Set bit7 and add the repetition count.
							nXposCounter++;					// Point to next byte in the array.
							nRepetition = 0;				// Reset repetition counter.
							gbytTXbuffer[nXposCounter+3] = nCurrentPixelData;
							nXposCounter++;					// Point to next byte in the array.
						}
						else                                // No multiple pixels of similar value.
						{
							gbytTXbuffer[nXposCounter+3] = nCurrentPixelData;
							nXposCounter++;
						}
					} // if (nCurrentPixelData == nRefPixelData)
				}  // Loop index

				gbytTXbuffer[2] = nXposCounter;   // No. of bytes in the data packet, excluding start-of-line code, line number and payload length.

 				SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
 										// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
 										// from the cache, it may not contains the correct and up-to-date data.  
 				XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
 				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nXposCounter+3);		// Set number of bytes to transmit = 14 bytes including payload.
 				XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
 				gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
 				gSCIstatus.bTXRDY = 1;										// Initiate TX.
 				PIN_LED2_SET;												// Lights up indicator LED2.

				nLineCounter++;						// Point to next line of image pixel data.
				if (nLineCounter == gnImageHeight)	// Check if reach end of line.
				{
					gnSendSecondaryInfo = 1;		// Set flag to transmit secondary info to host at the end of each frame.
					nLineCounter = 0;				// Reset line counter.
				}

				OSSetTaskContext(ptrTask, 4, 1);    // Next state = 4, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);    // Next state = 1, timer = 1.
			} // if (gSCIstatus.bTXRDY == 0)
			break;
			
			case 3: // State 3 - Send a line of pixel data, without any compression.
			// At the moment no compression is only allowed if we are sending the data contains in
			// image processing result buffer, gunIPResult[][].
			if (gSCIstatus.bTXRDY == 0)					// Check if  UART port is busy.
			{
				gbytTXbuffer[0] = 0xFF;					// Start of line code.
				gbytTXbuffer[1] = nLineCounter;			// Line number.
				// gbytTXbuffer[2] stores the payload length.
				nXposCounter = 0;						// Initialize x-position to first pixel.
				
				// Get all the bytes/pixels in the line of pixels data.
				for (nIndex = 0; nIndex < gnImageWidth; nIndex++)
				{
					
					nTemp = nIndex / 4;				// Shrink the column index by 4, because each unsigned integer contains 4 bytes.
					switch (nIndex % 4)				// The unsigned integer format is 32-bits in ARM Cortex-M.  However each pixel
					// computation result is 8-bits, thus each unsigned integer word can store results
					// for 4 pixels.
					// Bit 0-7:		pixel 1
					// Bit 8-15:	pixel 2
					// Bit 16-23:	pixel 3
					// Bit 24-31:	pixel 4
					{
						case 0:	// nIndex % 4 = 0	(nIndex = 0,4,8 etc)
						nCurrentPixelData = gunIPResult[nTemp][nLineCounter] & 0x000000FF;	// Get the lowest 8-bits from 32-bits word.
						break;
						
						case 1: // nIndex % 4 = 1	(nIndex = 1,5,9 etc)
						nCurrentPixelData = (gunIPResult[nTemp][nLineCounter] & 0x0000FF00)>>8;	// Get the 2nd 8-bits from 32-bits word.
						break;
						
						case 2: // nIndex % 4 = 2	(nIndex = 2,6,10 etc)
						nCurrentPixelData = (gunIPResult[nTemp][nLineCounter] & 0x00FF0000)>>16;	// Get the 3rd 8-bits from 32-bits word.
						break;
						
						default: // nIndex % 4 = 3	(nIndex = 3,7,11 etc)
						nCurrentPixelData = gunIPResult[nTemp][nLineCounter]>>24;			// Get the highest 8-bits from 32-bits word.
						break;
					}
					gbytTXbuffer[nXposCounter+3] = nCurrentPixelData;
					nXposCounter++;
				}  // Loop index

				gbytTXbuffer[2] = nXposCounter;   // No. of bytes in the data packet, excluding start-of-line code, line number and payload length.

				SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
				// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
				// from the cache, it may not contains the correct and up-to-date data.
				XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nXposCounter+3);		// Set number of bytes to transmit = 14 bytes including payload.
				XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
				gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
				gSCIstatus.bTXRDY = 1;										// Initiate TX.
				PIN_LED2_SET;												// Lights up indicator LED2.

				nLineCounter++;						// Point to next line of image pixel data.
				if (nLineCounter == gnImageHeight)	// Check if reach end of line.
				{
					gnSendSecondaryInfo = 1;		// Set flag to transmit secondary info to host at the end of each frame.
					nLineCounter = 0;				// Reset line counter.
				}
				OSSetTaskContext(ptrTask, 4, 1);    // Next state = 4, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);    // Next state = 1, timer = 1.
			} // if (gSCIstatus.bTXRDY == 0)
			break;			
			
			case 4: // State 4 - Wait for transmission of line pixel to end.
			
			if (gSCIstatus.bTXRDY == 0)	// Check if still got any data to send via UART.
			{
				if (gnSendSecondaryInfo == 1)			// Check if any secondary info to transmit to remote display.
				{
					OSSetTaskContext(ptrTask, 5, 1);     // Next state = 5, timer = 1.
				}
				else                                    // No secondary info to send to remote display, next line.
				{
					OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
				}
			}
			else  // Yes, still has pending data to send via UART.
			{
				OSSetTaskContext(ptrTask, 4, 1);     // Next state = 4, timer = 1.
			}
			break;

			case 5: // State 5 - Wait for start signal from remote host.  The start signal is the character
			// 'L' (for luminance data using RGB)
			// 'R' (for luminance data using R only)
			// 'G' (for luminance data using G only)
			// 'B' (for luminance data using B only)
			// 'D' (for gradient data).
			// 'H' (for hue data).
			// 'P' (for image processing result buffer data)
			if (gSCIstatus.bRXRDY == 1)						// Check if UART receive any data.
			{
				if (gSCIstatus.bRXOVF == 0)					// Make sure no overflow error.
				{
					bytData = gbytRXbuffer[0];				// Get 1 byte and ignore all others.
					OSSetTaskContext(ptrTask, 6, 1);		// Next state = 6, timer = 1.
				}
				else
				{
					gSCIstatus.bRXOVF = 0; 					// Reset overflow error flag.
					OSSetTaskContext(ptrTask, 1, 1);		// Next state = 1, timer = 1.
				}
				gSCIstatus.bRXRDY = 0;						// Reset valid data flag.
				gbytRXbufptr = 0; 							// Reset pointer.
				PIN_LED2_CLEAR;								// Turn off indicator LED2.
			}
			else
			{
				OSSetTaskContext(ptrTask, 5, 1);			// Next state = 5, timer = 1.
			}
			break;

			case 6: // State 6 - Send auxiliary data (markers, texts etc).			
			gbytTXbuffer[0] = 0xFF;					// Start of line code.
			gbytTXbuffer[1] = 254;					// Line number of 254 indicate auxiliary data.
			gbytTXbuffer[2] = 13;					// Payload length is 13 bytes.
			
			gbytTXbuffer[3] = gobjRec1.nWidth;		// 0. Get the info for rectangle to be highlighted in remote display.
			gbytTXbuffer[4] = gobjRec1.nHeight;		// 1. Height and width.
			gbytTXbuffer[5] = gobjRec1.nX;			// 2. Starting point (top left hand corner) along x and y axes.
			gbytTXbuffer[6] = gobjRec1.nY;			// 3.
			gbytTXbuffer[7] = gobjRec1.nColor;		// 4. Set the color of the marker 1 .
			gbytTXbuffer[8] = gobjRec2.nWidth;		// 0. Get the info for rectangle to be highlighted in remote display.
			gbytTXbuffer[9] = gobjRec2.nHeight;		// 1. Height and width.
			gbytTXbuffer[10] = gobjRec2.nX;			// 2. Starting point (top left hand corner) along x and y axes.
			gbytTXbuffer[11] = gobjRec2.nY;			// 3.
			gbytTXbuffer[12] = gobjRec2.nColor;		// 4. Set the color of the marker 2.
			//gbytTXbuffer[13] = gunHue;				// Some tag along parameter for debugging purpose.
			gbytTXbuffer[13] = gunAverageLuminance;		// Some tag along parameter for debugging purpose.
			gbytTXbuffer[14] = gunRunIPAInterval1;
			gbytTXbuffer[15] = gunRunIPAInterval2;

 			SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
 									// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
 									// from the cache, it may not contains the up-to-date and correct data.  
 			XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
 			XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(16);		// Set number of bytes to transmit = 16 bytes including payload.
 			XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
 			gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
 			gSCIstatus.bTXRDY = 1;										// Initiate TX.
 			PIN_LED2_SET;												// Lights up indicator LED2.
			
			OSSetTaskContext(ptrTask, 7, 1);		// Next state = 7, timer = 1.
			break;

			case 7: // State 7 - Wait for transmission of line pixel to end.
			if (gSCIstatus.bTXRDY == 0)	// Check if still any data to send via UART.
			{
				gnSendSecondaryInfo = 0;			// Clear flag.
				OSSetTaskContext(ptrTask, 1, 1);    // Next state = 1, timer = 1.
			}
			else  // Yes, still has pending data to send via UART.
			{
				OSSetTaskContext(ptrTask, 7, 1);    // Next state = 7, timer = 1.
			}
			break;
			
			case 10: // State 10 - Reset HC-05 Bluetooth module (if attached).  Note that if we keep the HC-05 module in
			// reset state, it will consume little power.  This trick can be used when we wish to power down
			// HC-05 to conserve power.
			PIN_HC_05_RESET_SET;					// Reset Bluetooth module.
			OSSetTaskContext(ptrTask, 11, 2*__NUM_SYSTEMTICK_MSEC);     // Next state = 11, timer = 2 msec.
			break;
			
			case 11: // State 11 - Reset HC-05 Bluetooth module.
			PIN_HC_05_RESET_CLEAR;					// Clear Reset to Bluetooth module.
			OSSetTaskContext(ptrTask, 12, 100*__NUM_SYSTEMTICK_MSEC);     // Next state = 12, timer = 100 msec.
			break;
			
			case 12: // State 12 - Clear USART0 after HC-05 Bluetooth module reset.
				gSCIstatus2.bRXOVF = 0; 		// Reset overflow error flag.
				gSCIstatus2.bRXRDY = 0;			// Reset valid data flag.
				gbytRXbufptr2 = 0; 				// Reset pointer.
				PIN_LED2_CLEAR;					// Turn off indicator LED2.
				OSSetTaskContext(ptrTask, 1, 1*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 1 msec.
			break;
			
			default:
			OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}


///
/// Function name	: Proce_RunImageProcess 
///
/// Author			: Fabian Kung
///
/// Last modified	: 14 October 2019
///
/// Code Version	: 0.60
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			: None
///
/// MODULES			: None
///
/// RTOS			: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    :

#ifdef __OS_VER			// Check RTOS version compatibility.
#if __OS_VER < 1
#error "Proce_RunImageProcess: Incompatible OS version"
#endif
#else
#error "Proce_RunImageProcess: An RTOS is required with this function"
#endif

///
/// Description	:
/// Dispatcher for image processing algorithms.  Each algorithm will work on an image frame until
/// the algorithm completes.

void Proce_RunImageProcess(TASK_ATTRIBUTE *ptrTask)
{
	static	int	nCurrentFrame;
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - General initialization.
				//gunRunIPAInterval1 = 3;
				//gunRunIPAInterval2 = 3;
				OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 1, timer = 1000 msec.
			break;

			case 1: // State 1 - Wait for the camera driver ready signal.
			if (gnCameraReady == _CAMERA_READY)
			{
				nCurrentFrame = gnFrameCounter;		// Update image frame counter.
				gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_IDLE;	// Initialize image processing algorithm busy flag.	
				OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
			}
			break;

			case 2: // State 2 - Wait for start of new frame.  If a new frame is detected, send any data in the transmit buffer
					// then prepare to execute image processing algorithm.
			if (gnFrameCounter != nCurrentFrame)			// Check if new image frame has been captured.
			{
				nCurrentFrame = gnFrameCounter;				// Update current frame counter.
				gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_BUSY;			// Indicate an IPA will be run soon.
				
				if (gSCIstatus2.bTXRDY == 0)				// Check if any data to send via USART is busy.
				{											// If not busy check if there is any valid data to be transmitted to remote controller.
					if (gbytTXbuffer2[0] > 0)				// Byte0 is the algorithm ID, 0 means no IPA is active.
					{
						gbytTXbuflen2 = 4;					// Set TX frame length.
						gSCIstatus2.bTXRDY = 1;				// Initiate TX.
					}		
				}				
				
				OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.
			}
			else                                            // Is still old frame, keep polling.
			{
				OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
			}			
			break;
			
			case 3: // State 3 - Start executing 1st image processing algorithm (IPA), the IPA will 
					// clear the gnImageProcessingAlgorithmBusy flag once it complete the analysis of an
					// image frame.
			if (gnImageProcessingAlgorithmBusy != _IMAGEPROCESSINGALGORITHM_IDLE)	
			{
				switch (gunRunIPAInterval1)
				{	
					case 1: 
						ImageProcessingAlgorithm1();
						OSSetTaskContext(ptrTask, 3, 1);    // Next state = 3, timer = 1.
					break;					
					
					case 2:
						ImageProcessingAlgorithm2();
						OSSetTaskContext(ptrTask, 3, 1);    // Next state = 3, timer = 1.
					break;
					
					case 3:
						ImageProcessingAlgorithm3();
						OSSetTaskContext(ptrTask, 3, 1);    // Next state = 3, timer = 1.
					break;
					
					default: // No image processing algorithm to execute, go to next interval.
						gnCameraLED = 0;					// Turn off camera LED.
						gbytTXbuffer2[0] = 0;				// Set 1st byte of gbtyTXbuffer2 to 0 as Byte0 is 
															// used to indicate if a valid IPA has executed or not.
															// A value of zero means no IPA has run previously and the
															// data in the transmit buffer will not be erroneously 
															// send out.
						OSSetTaskContext(ptrTask, 4, 1);    // Next state = 4, timer = 1.
					break;
				}				
			}
			else  // gnImageProcessingAlgorithmBusy == _IMAGEPROCESSINGALGORIHTM_IDLE, image processing completes,
				  // to wait for new frame to begin.
			{
				OSSetTaskContext(ptrTask, 4, 1);    // Next state = 4, timer = 1.	
			}
			break;
			
			case 4: // State 4 - Wait for start of new frame.
			if (gnFrameCounter != nCurrentFrame)			// Check if new image frame has been captured.
			{
				nCurrentFrame = gnFrameCounter;				// Update current frame counter.
				gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_BUSY;			// Indicate an IPA will be run soon.
				
				if (gSCIstatus2.bTXRDY == 0)				// Check if any data to send via USART is busy.
				{											// If not busy check if there is any valid data to be transmitted to remote controller.
					if (gbytTXbuffer2[0] > 0)				// Byte0 is the algorithm ID, 0 means no IPA is active.
					{
						gbytTXbuflen2 = 4;					// Set TX frame length.
						gSCIstatus2.bTXRDY = 1;				// Initiate TX.
					}
				}				
				OSSetTaskContext(ptrTask, 5, 1);			// Next state = 5, timer = 1.
			}
			else                                            // Is still old frame, keep polling.
			{
				OSSetTaskContext(ptrTask, 4, 1);			// Next state = 4, timer = 1.
			}
			break;			

			case 5: // State 5 - Start executing 2nd image processing algorithm (IPA), the IPA will
			// clear the gnImageProcessingAlgorithmBusy flag once it complete the analysis of an
			// image frame.
			if (gnImageProcessingAlgorithmBusy != _IMAGEPROCESSINGALGORITHM_IDLE)
			{
				switch (gunRunIPAInterval2)
				{
					case 1:
					ImageProcessingAlgorithm1();
					OSSetTaskContext(ptrTask, 5, 1);    // Next state = 5, timer = 1.
					break;					
					
					case 2:
					ImageProcessingAlgorithm2();
					OSSetTaskContext(ptrTask, 5, 1);    // Next state = 5, timer = 1.
					break;
					
					case 3:
					ImageProcessingAlgorithm3();
					OSSetTaskContext(ptrTask, 5, 1);    // Next state = 5, timer = 1.
					break;
					
					default: // No image processing algorithm to execute, go to next interval.
					gnCameraLED = 0;					// Turn off camera LED.
					gbytTXbuffer2[0] = 0;				// Set 1st byte of gbtyTXbuffer2 to 0 as Byte0 is
														// used to indicate if a valid IPA has executed or not.
														// A value of zero means no IPA has run previously and the
														// data in the transmit buffer will not be erroneously
														// send out.					
					OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
					break;
				}
			}
			else  // gnImageProcessingAlgorithmBusy == _IMAGEPROCESSINGALGORIHTM_IDLE, image processing completes,
			// to wait for new frame to begin.
			{
				OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
			}
			break;			
			
			default:
				OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}


///
/// Function name	: ImageProcessingAlgorithm2
///
/// Author			: Fabian Kung
///
/// Last modified	: 19 Sep 2018
///
/// Code Version	: 0.86
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			:
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
///                       gunAverageLuminance
///
/// Description	:
/// 1. Perform mathematical morphology on the ROI in the image using LBP (local binary pattern)
/// approach.  Here we assume a binary image (black and white) with threshold be selected properly.
/// The threshold is the average luminance level of each frame.
/// The mathematical morphology procedure helps to de-noise the binary image.
/// 2. After de-noising, we inspect the ROI, whenever it contains a number of black pixels above
/// a certain threshold, we interpret that as object present in the ROI.
/// 3. Here we have 9 ROIs, 3 each on the left, middle and on the right vision field, to ascertain
/// if there are objects in the left, front, or right of the camera.
/// 4. The process will report the status of each ROIs to the remote controller via the
/// following protocol:
///
/// The status of each row of ROIs is represented by 1 byte, as follows:
/// bit2: 1 = Left ROI contain objects, else 0.
/// bit1: 1 = Middle ROI contain objects, else 0.
/// bit0: 1 = Right ROI contain objects, else 0.
///
/// A 4 bytes code is transmitted via USART to the remote controller to
/// indicate the status of the machine vision module.
/// Byte 0: The process ID (PID) of this process (e.g. 2).
/// Byte 1: The status of ROIs row 2.
/// Byte 2: The status of ROIs row 1.
/// Byte 3: The status of ROIs row 0.
/// Note that we transmit the last row first! (Big endian)
/// Thus a packet from this process is as follows:
/// [PID] + [ROW3 status] + [ROW2 status] + [ROW1 status]
///
/// The ROI is sub-divided into 9 sub-regions, as follows:
///
/// ----------------------------> x (horizontal)
/// |
/// |     ROI00	ROI10 ROI20
/// |    ROI01	ROI11  ROI21
/// |   ROI02	ROI12	ROI22
/// |  ROI03    ROI13    ROI22
/// |
/// V
/// y (vertical)
///
/// nROIStartX and nROIStartY: set the start pixel (x,y) position.
/// nROIWIdth and nROIHeight: set the width and height of each sub-region.
/// nROIXOffset: This is an offset value added to nROIStartX, to cater for
/// the inherent mechanical error in the azimuth direction.  Due to the limited
/// resolution of the coupling, sometimes the camera is not pointing straight
/// ahead even though the azimuth angle of the motor is set to zero.  We can
/// adjust this offset to move all the ROIs to the left or right.

#define     __IP2_MAX_ROIX		3		// Set the number of ROI along x axis.
#define     __IP2_MAX_ROIY		4		// Set the number of ROI along y axis.
#define		__IP2_ROI00_XSTART	53		// Start position for ROI (upper left corner
#define		__IP2_ROI00_YSTART	79		// coordinate) in pixels.
#define     __IP2_ROI01_XSTART  45
#define		__IP2_ROI01_YSTART  88
#define		__IP2_ROI02_XSTART  39
#define		__IP2_ROI02_YSTART  97
#define		__IP2_ROI03_XSTART  32
#define		__IP2_ROI03_YSTART  106
#define		__IP2_ROI00_WIDTH	18
#define		__IP2_ROI01_WIDTH	23		// The width of a single ROI region in pixels.
#define		__IP2_ROI02_WIDTH	27
#define		__IP2_ROI03_WIDTH	32
#define		__IP2_ROI_WIDTH_TOTAL	 __IP2_MAX_ROIX*__IP2_ROI_WIDTH	// Total ROI width
#define		__IP2_ROI_HEIGHT	9		// The height of a single ROI region in pixels.
#define		__IP2_ROI_HEIGHT_TOTAL	__IP2_MAX_ROIY*__IP2_ROI_HEIGHT
#define     __IP2_ROI_THRESHOLD 10		// The no. of pixels to trigger the object recognition.
//#define     __IP2_ROIX_START_OFFSET 10	// For V2T2A camera
#define     __IP2_ROIX_START_OFFSET 0	// For V2T1A camera

typedef struct StructSquareROI
{
	int nStartX;				// Start pixel coordinate (top left hand corner).
	int nStartY;				//
	unsigned int unWidth;		// Width and height of ROI.
	unsigned int unHeigth;
	
} SQUAREROI;

void ImageProcessingAlgorithm2(void)
{
	static	int	nTimer = 1;		// Start timer with 1.
	static	int nState = 0;
	
	static	int	nCurrentFrame;
	static	int nXindex, nYindex;
	static	int	nLumReference;
	
	static unsigned int	unLumCumulativeROI = 0;
	static unsigned int unAverageLumROI = 0;
	static int	nPixelCount = 0;
	
	int nLuminance;
	int nTemp;
	int	nColEnd, nColStart;
	int	nRowEnd, nRowStart;
	static	int ni = 0;
	static	int nj = 0;
	//int nColPixelCount;
	static int nROIThreshold[3][4];		// The region-of-interest (ROI) threshold array.
	//static int nROIStartX, nROIStartY;
	//static int nROIWidth; 
	//static int nROIHeight;
	static int nROIXOffset;
	
	static	SQUAREROI	objROI[3][4];	// The region-of-interest (ROI) grid.

	nTimer--;							// Decrement timer.
	
	if (nTimer == 0)
	{
		switch (nState)
		{
			case 0: // State 0 - Power-up initialization, initialize each ROI parameters.
			objROI[0][0].nStartX = __IP2_ROI00_XSTART;
			objROI[0][0].nStartY = __IP2_ROI00_YSTART;
			objROI[0][0].unWidth = __IP2_ROI00_WIDTH;
			objROI[0][0].unHeigth = __IP2_ROI_HEIGHT;
			objROI[1][0].nStartX = __IP2_ROI00_XSTART + __IP2_ROI00_WIDTH;
			objROI[1][0].nStartY = __IP2_ROI00_YSTART;
			objROI[1][0].unWidth = __IP2_ROI00_WIDTH;
			objROI[1][0].unHeigth = __IP2_ROI_HEIGHT;
			objROI[2][0].nStartX = __IP2_ROI00_XSTART + (2*__IP2_ROI00_WIDTH);
			objROI[2][0].nStartY = __IP2_ROI00_YSTART;
			objROI[2][0].unWidth = __IP2_ROI00_WIDTH;
			objROI[2][0].unHeigth = __IP2_ROI_HEIGHT;
			objROI[0][1].nStartX = __IP2_ROI01_XSTART;
			objROI[0][1].nStartY = __IP2_ROI01_YSTART;
			objROI[0][1].unWidth = __IP2_ROI01_WIDTH;
			objROI[0][1].unHeigth = __IP2_ROI_HEIGHT;
			objROI[1][1].nStartX = __IP2_ROI01_XSTART + __IP2_ROI01_WIDTH;
			objROI[1][1].nStartY = __IP2_ROI01_YSTART;
			objROI[1][1].unWidth = __IP2_ROI01_WIDTH;
			objROI[1][1].unHeigth = __IP2_ROI_HEIGHT;
			objROI[2][1].nStartX = __IP2_ROI01_XSTART + (2*__IP2_ROI01_WIDTH);
			objROI[2][1].nStartY = __IP2_ROI01_YSTART;
			objROI[2][1].unWidth = __IP2_ROI01_WIDTH;
			objROI[2][1].unHeigth = __IP2_ROI_HEIGHT;
			objROI[0][2].nStartX = __IP2_ROI02_XSTART;
			objROI[0][2].nStartY = __IP2_ROI02_YSTART;
			objROI[0][2].unWidth = __IP2_ROI02_WIDTH;
			objROI[0][2].unHeigth = __IP2_ROI_HEIGHT;
			objROI[1][2].nStartX = __IP2_ROI02_XSTART + __IP2_ROI02_WIDTH;
			objROI[1][2].nStartY = __IP2_ROI02_YSTART;
			objROI[1][2].unWidth = __IP2_ROI02_WIDTH;
			objROI[1][2].unHeigth = __IP2_ROI_HEIGHT;
			objROI[2][2].nStartX = __IP2_ROI02_XSTART + (2*__IP2_ROI02_WIDTH);
			objROI[2][2].nStartY = __IP2_ROI02_YSTART;
			objROI[2][2].unWidth = __IP2_ROI02_WIDTH;
			objROI[2][2].unHeigth = __IP2_ROI_HEIGHT;
			objROI[0][3].nStartX = __IP2_ROI03_XSTART;
			objROI[0][3].nStartY = __IP2_ROI03_YSTART;
			objROI[0][3].unWidth = __IP2_ROI03_WIDTH;
			objROI[0][3].unHeigth = __IP2_ROI_HEIGHT;
			objROI[1][3].nStartX = __IP2_ROI03_XSTART + __IP2_ROI03_WIDTH;
			objROI[1][3].nStartY = __IP2_ROI03_YSTART;
			objROI[1][3].unWidth = __IP2_ROI03_WIDTH;
			objROI[1][3].unHeigth = __IP2_ROI_HEIGHT;
			objROI[2][3].nStartX = __IP2_ROI03_XSTART + (2*__IP2_ROI03_WIDTH);
			objROI[2][3].nStartY = __IP2_ROI03_YSTART;
			objROI[2][3].unWidth = __IP2_ROI03_WIDTH;
			objROI[2][3].unHeigth = __IP2_ROI_HEIGHT;						
			nState = 1;		// next state = 1, delay = 1 tick.						
			nTimer = 1;
			break; 
			
			case 1: // State 1 - Continue with initialization.
				PIN_FLAG3_SET;							// Set flag, this is optional, for debugging purpose.
				gnCameraLED = 1;						// Set camera LED intensity to lowest.
				unAverageLumROI = gunAverageLuminance;	// Initialize the local average luminance as equal to global (e.g.
														// whole frame) average luminance.
				nLumReference = 127;					// Set this to the maximum! (for 7 bits unsigned integer)
				nPixelCount = 0;						// Reset pixel counter.
				nYindex = __IP2_ROI00_YSTART-1;			// Set the y start position for the de-noising process.
				nROIXOffset =  __IP2_ROIX_START_OFFSET;
				// Calibration or offset in x pixels (This value can
				// be positive or negative).  Due to the resolution of
				// the motor controlling the azimuth angle, sometime
				// the camera is not pointing straight ahead even though
				// the azimuth angle is zero.
				nXindex = __IP2_ROI00_XSTART+__IP2_ROIX_START_OFFSET;	// Set the x start position for the de-noising process.
				ni = 0;
				nj = 0;				
				nState = 3;								// next state = 3, delay = 1 tick.
				nTimer = 1;
			break;

			case 3: // State 3 - Reduce the grayscale in the ROI to 2 for the image frame, one ROI at a time until all ROIs are processed.			
			
			nRowStart = objROI[ni][nj].nStartY;							// Set up the start (x,y) pixel coordinates, and
			nColStart = objROI[ni][nj].nStartX;							// the end limits of the current ROI.
			nRowEnd = nRowStart + objROI[ni][nj].unHeigth;
			nColEnd = nColStart + objROI[ni][nj].unWidth;
			
			for (nYindex = nRowStart; nYindex < nRowEnd; nYindex++)
			{
				for (nXindex = nColStart; nXindex < nColEnd; nXindex++)
				{
					if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
					{												// is valid for access.						
						nLuminance = gunImgAtt2[nXindex][nYindex] & _LUMINANCE_MASK;		// Extract the luminance for current pixel from frame buffer 2.
						if (nLuminance >= unAverageLumROI)						// If current pixel is white.
						{
							gunImgAtt2[nXindex][nYindex] = (gunImgAtt2[nXindex][nYindex] & _CLUMINANCE_MASK) + nLumReference;	// Set bit6-0 to value of nLumReference;
						}
						else													// If current pixel is black.
						{
							gunImgAtt2[nXindex][nYindex] = gunImgAtt2[nXindex][nYindex] & _CLUMINANCE_MASK;	// Set bit6-0 to 0;
						}						
					}
					else
					{						
						nLuminance = gunImgAtt[nXindex][nYindex] & _LUMINANCE_MASK;		// Extract the luminance for current pixel from frame buffer 1.
						if (nLuminance >= unAverageLumROI)						// If current pixel is white.
						{
							gunImgAtt[nXindex][nYindex] = (gunImgAtt[nXindex][nYindex] & _CLUMINANCE_MASK) + nLumReference;	// Set bit6-0 to value of nLumReference;
						}
						else													// If current pixel is black.
						{
							gunImgAtt[nXindex][nYindex] = gunImgAtt[nXindex][nYindex] & _CLUMINANCE_MASK;	// Set bit6-0 to 0;
						}												
					}					
									
					unLumCumulativeROI = unLumCumulativeROI + nLuminance;	// Accumulate the luminance within ROI.
					nPixelCount++;											// Increment the pixel counter;				
				}
			}
			
			ni++;														// Proceed to next ROI.
			if (ni == __IP2_MAX_ROIX)									// Increment the x and y indices of the ROI.
			{
				ni = 0;
				nj++;
				if (nj == __IP2_MAX_ROIY)
				{
					nj = 0;
					unAverageLumROI = unLumCumulativeROI/nPixelCount;		// Compute average luminance in the ROI.
					unLumCumulativeROI = 0;									// Reset ROI cumulative luminance.
					nPixelCount = 0;										// Reset the pixel counter.
					//OSSetTaskContext(ptrTask, 4, 1);						// Next state = 4, timer = 1.
					nState = 4;												// Next state = 4, timer = 1 tick.
					nTimer = 1;
				}
				else
				{
					//OSSetTaskContext(ptrTask, 3, 1);						// Next state = 3, timer = 1.
					nState = 3;												// Next state = 3, timer = 1 tick.
					nTimer = 1;					
				}
			}
			else
			{
				nState = 3;													// Next state = 3, timer = 1 tick.
				nTimer = 1;
			}
			break;

			case 4: // State 4 - Check if any object is detected (e.g. luminance is 0) within each sub-ROI.		
			nRowStart = objROI[ni][nj].nStartY;							// Set up the start (x,y) pixel coordinates, and
			nColStart = objROI[ni][nj].nStartX;							// the end limits of the current ROI.
			nRowEnd = nRowStart + objROI[ni][nj].unHeigth;
			nColEnd = nColStart + objROI[ni][nj].unWidth;

			nROIThreshold[ni][nj] = 0;
			
			for (nYindex = nRowStart; nYindex < nRowEnd; nYindex++)
			{
				for (nXindex = nColStart; nXindex < nColEnd; nXindex++)
				{
					if (gnValidFrameBuffer == 1)				// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
					{											// is valid for access.					
						if ((gunImgAtt2[nXindex][nYindex] & _LUMINANCE_MASK) < nLumReference)	// Check for 'dark spots' in ROI.
						{
							nROIThreshold[ni][nj]++;	// Update the counter for 'dark spots'.
						}
					}
					else
					{
						if ((gunImgAtt[nXindex][nYindex] & _LUMINANCE_MASK) < nLumReference)	// Check for 'dark spots' in ROI.
						{
							nROIThreshold[ni][nj]++;	// Update the counter for 'dark spots'.
						}												
					}
				}
			}
            
			ni++;														// Proceed to next ROI.
			if (ni == __IP2_MAX_ROIX)									// Increment the x and y indices of the ROI.
			{
				ni = 0;
				nj++;
				if (nj == __IP2_MAX_ROIY)
				{
					nj = 0;
					//OSSetTaskContext(ptrTask, 6, 1);						// Next state = 6, timer = 1.
					nState = 6;												// Next state = 6, timer = 1 tick.
					nTimer = 1;					
				}
				else
				{
					//OSSetTaskContext(ptrTask, 4, 1);						// Next state = 4, timer = 1.
					nState = 4;												// Next state = 4, timer = 1 tick.
					nTimer = 1;					
				}
			}
			else
			{
				nState = 4;												// Next state = 4, timer = 1 tick.
				nTimer = 1;				
			}
			break;

			case 6: // State 6 - Check if any object detected (e.g. luminance is 0) within the ROIs, report to remote controller.
			//
			// The status of each row of ROIs is represented by 1 byte, as follows:
			// bit2: 1 = Left ROI contain objects, else 0.
			// bit1: 1 = Middle ROI contain objects, else 0.
			// bit0: 1 = Right ROI contain objects, else 0.
			//
			// A 4 bytes code is transmitted via USART to indicate the status of the machine vision module.
			// Byte 0: The process ID (PID) of this process (e.g. 2).
			// Byte 1: The status of ROIs row 2.
			// Byte 2: The status of ROIs row 1.
			// Byte 3: The status of ROIs row 0.
			// Note that we transmit the last row first! (Big endian)
			// Thus a packet from this process is as follows:
			// [PID] + [ROW2 status] + [ROW1 status] + [ROW0 status]
			//
			// NOTE: 28 Dec 2016.  To prevent overflow error on the remote controller (as it does not use DMA on the UART),
			// we avoid sending all 4 bytes one shot, but split the data packet into two packets or 2 bytes each.  A 1 msec
			// delay is inserted between each two bytes packet.  As the algorithm in the remote controller improves, this
			// artificial restriction can be removed.

			//gobjRec1.nHeight = __IP2_ROI_HEIGHT*__IP2_MAX_ROIY;		// Mark the ROI.
			//gobjRec1.nWidth = __IP2_ROI00_WIDTH*__IP2_MAX_ROIX;
			//gobjRec1.nX = __IP2_ROI00_XSTART + nROIXOffset;
			//gobjRec1.nY = __IP2_ROI00_YSTART;
			//gobjRec1.nColor = 130;						// Set ROI to green, no fill, 128 + 2 = 130.
			
			// Bit7 = 1 - No fill.
			// Bit7 = 0 - Fill.
			// Bit0-6 = 1 to 127, determines color.
			// If nColor = 0, the ROI will not be displayed in the remote monitor.

			// 27 July 2017: The scenario considered here:
			// (a) When the floor is bright or light color than the surrounding.  In this case the floor
			// will appear as white pixels and the surrounding as black pixels.  Thus any black pixels
			// within the floor area will be interpreted as objects. The threshold should not be more than
			// 5% of the total pixels within the ROI.

				nTemp = 0;
			
				// Check left thresholds.
				if (nROIThreshold[0][2] > __IP2_ROI_THRESHOLD)
				//if ((nROIThreshold[0][2] > __IP2_ROI_THRESHOLD) && (nROIThreshold[0][2] < 200))
				{	
					nTemp = 0x04;				// Set bit 2.
				}
				// Check middle threshold.
				if (nROIThreshold[1][2] > __IP2_ROI_THRESHOLD)
				//if ((nROIThreshold[1][2] > __IP2_ROI_THRESHOLD) && (nROIThreshold[1][2] < 200))
				{
					nTemp = nTemp | 0x02;		// Set bit 1.
				}
				// Check right threshold.
				if (nROIThreshold[2][2] > __IP2_ROI_THRESHOLD)
				//if ((nROIThreshold[2][2] > __IP2_ROI_THRESHOLD) && (nROIThreshold[2][2] < 200))
				{
					nTemp = nTemp | 0x01;		// Set bit 0.
				}
				gbytTXbuffer2[1] = nTemp;		// Load data.

				gbytTXbuffer2[0] = 2;		//Load image processing algorithm ID.
				nState = 7;					// Next state = 7, timer = 1 msec.
				nTimer = 1*__NUM_SYSTEMTICK_MSEC;				
			break;

			case 7: // State 7 - Continue checking if any object detected (e.g. luminance is 0) within the right ROI and report
			// status to remote controller.
			nTemp = 0;
			// Check left thresholds.
			if (nROIThreshold[0][1] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[0][1] > __IP2_ROI_THRESHOLD) && (nROIThreshold[0][1] < 200))
			{
				nTemp = 0x04;				// Set bit 2.
			}
			// Check middle threshold.
			if (nROIThreshold[1][1] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[1][1] > __IP2_ROI_THRESHOLD) && (nROIThreshold[1][1] < 200))
			{			
				nTemp = nTemp | 0x02;		// Set bit 1.
			}
			// Check right threshold.
			if (nROIThreshold[2][1] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[2][1] > __IP2_ROI_THRESHOLD) && (nROIThreshold[2][1] < 200))
			{		
				nTemp = nTemp | 0x01;		// Set bit 0.
			}
			gbytTXbuffer2[2] = nTemp;		// Load data.

			nTemp = 0;
			// Check left thresholds.
			if (nROIThreshold[0][0] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[0][0] > __IP2_ROI_THRESHOLD) && (nROIThreshold[0][0] < 200))
			{			
				nTemp = 0x04;				// Set bit 2.
			}
			// Check middle threshold.
			if (nROIThreshold[1][0] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[1][0] > __IP2_ROI_THRESHOLD) && (nROIThreshold[1][0] < 200))
			{			
				nTemp = nTemp | 0x02;		// Set bit 1.
			}
			// Check right threshold.
			if (nROIThreshold[2][0] > __IP2_ROI_THRESHOLD)
			//if ((nROIThreshold[2][0] > __IP2_ROI_THRESHOLD)	&& (nROIThreshold[2][0] < 200))
			{
				nTemp = nTemp | 0x01;		// Set bit 0.
			}
			gbytTXbuffer2[3] = nTemp;		// Load data.

			nState = 8;					// Next state = 8, timer = 1 msec.
			nTimer =  1*__NUM_SYSTEMTICK_MSEC;				
			break;

			case 8: // State 8 - End, clear busy flag to let other processes know that current algorithm completes.
				gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_IDLE;
				nState = 1;					// Next state = 1, timer = 1 tick.
				nTimer = 1;				
				PIN_FLAG3_CLEAR;			// Clear flag, this is optional, for debugging purpose.
			break;

			default:
				//OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
				nState = 0;					// Next state = 0, timer = 1 tick.
				nTimer = 1;			
			break;
		} // switch (nState)
	} // if (nTimer == 0)
}


///
/// Function name	: ImageProcessingAlgorithm3
///
/// Author			: Fabian Kung
///
/// Last modified	: 9 Sep 2019
///
/// Code Version	: 0.82
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
#define		_IP3_HUEOFINTEREST_HIGHLIMIT_YELLOWGREEN	80		// Between 0 to 359, refer to hue table.
#define		_IP3_HUEOFINTEREST_LOWLIMIT_YELLOWGREEN		60		// 
//#define		_IP3_VALID_PIXEL_THRESHOLD		3
#define		_IP3_VALID_PIXEL_THRESHOLD		2
//#define		_IP3_VALID_PIXEL_THRESHOLD		1

void ImageProcessingAlgorithm3(void)
{
	static	int	nTimer = 1;		// Start timer with 1.
	static	int nState = 0;	
	
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
	
	nTimer--;							// Decrement timer.	
	
	if (nTimer == 0)
	{
		switch (nState)
		{
			case 0: // State 0 - Power-up initialization.
				nState = 1;					// Next state = 1, delay = 1 tick.
				nTimer = 1;
			break;

			case 1: // State 1 - Initialization at the start of new image frame in the buffer.
				PIN_FLAG4_SET;												// Set indicator flag, this is optional, for debugging purpose.
				nXindex = 1;												// Ignore 1st Column (i.e. Column 0).
				//gnCameraLED = 1;											// Set camera LED to low.
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
				
				nState = 2;					// Next state = 2, timer = 1 tick.
				nTimer = 1;				
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
				//OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				nState = 3;													// Next state = 3, timer = 1 tick.
				nTimer = 1;				
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
				//OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				nState = 3;													// Next state = 3, timer = 1 tick.
				nTimer = 1;								
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
				//OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				nState = 3;													// Next state = 3, timer = 1 tick.
				nTimer = 1;							
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
				//OSSetTaskContext(ptrTask, 3, 1);							// Next state = 3, timer = 1.
				nState = 3;													// Next state = 3, timer = 1 tick.
				nTimer = 1;				
			}
			else
			{
				//OSSetTaskContext(ptrTask, 2, 1);							// Next state = 2, timer = 1.
				nState = 2;													// Next state = 2, timer = 1 tick.
				nTimer = 1;				
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
				//OSSetTaskContext(ptrTask, 4, 1);							// Next state = 4, timer = 1.
				nState = 4;													// Next state = 4, timer = 1 tick.
				nTimer = 1;				
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
				//OSSetTaskContext(ptrTask, 4, 1);							// Next state = 4, timer = 1.
				nState = 4;													// Next state = 4, timer = 1 tick.
				nTimer = 1;				
				break;														// Exit current state.
			}				

			nState = 3;														// Next state = 3, timer = 1 tick.
			nTimer = 1;							
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
				gobjRec1.nX = nMaxCol;
				gobjRec1.nY = nMaxRow;
			}
			else
			{
				gnCameraLED = 1;												// Set camera LED to low, to signal that no object of interest (with matching color)
																				// is detected.
																				// Invalid column value.
				gobjRec1.nHeight = 0 ;											// Disable square marker to be displayed in remote monitor
				gobjRec1.nWidth = 0 ;											// software.
				gobjRec1.nX = 0;												// Set the location of the marker.
				gobjRec1.nY = 0;
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
			//OSSetTaskContext(ptrTask, 5, 1);									// Next state = 5, timer = 1.	
			nState = 5;															// Next state = 5, timer = 1 tick.
			nTimer = 1;								
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
				nState = 6;										// Next state = 6, timer = 1 msec.
				nTimer = 1*__NUM_SYSTEMTICK_MSEC;					
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
				nState = 7;							// Next state = 7, timer = 1 msec.
				nTimer = 1*__NUM_SYSTEMTICK_MSEC;				
			break;
			
			case 7: // State 7 - End, clear busy flag to let other processes know that current algorithm completes.
				gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_IDLE;
				PIN_FLAG4_CLEAR;						// Clear indicator flag.
				nState = 1;								// Next state = 1, timer = 1 tick.
				nTimer = 1;				
			break;
			
			default:
				//OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
				nState = 0;								// Next state = 0, timer = 1 tick.
				nTimer = 1;			
			break;
		}
	}
}


///
/// Function name	: ImageProcessingAlgorithm1
///
/// Author			: Fabian Kung
///
/// Last modified	: 11 Feb 2021
///
/// Code Version	: 0.99
///
/// Processor		: ARM Cortex-7 family
///
/// Processor/System Resource
/// PINS			: None
///
/// MODULES			: Camera driver.
///					  Proce_USART0_Driver
///
/// RTOS			: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    : gnCameraLED
///
/// Description	:
/// A simple image processing task to find brightest and darkest spots in an image frame.
/// Here we make use of the image luminance histogram from the camera driver to determine
/// the highest and lowest luminance values.
/// 1) Find the average coordinate for the brightest spot and the darkest spot.
/// 2) Send the coordinate of the brightest spot to external controller via USART0 port.

#define		_MAX_NUMBER_LUMINANCE_REF	2	// This sets the maximum number of coordinates to store for brightest spots.
// If it is 2 this means we will be storing the average coordinates for the
// brightest and the next brightest pixels in the frame.  At the moment this is
// only done for brightest pixels.  For pixel with lowest intensity, we only
// store the coordinate for lowest intensity.

void ImageProcessingAlgorithm1(void)
{
	static	int	nTimer = 1;		// Start timer with 1.
	static	int nState = 0;
	
	static	int	nCurrentFrame;
	static	int	nYindex, nXindex;
	static	int	nxmax[_MAX_NUMBER_LUMINANCE_REF], nymax[_MAX_NUMBER_LUMINANCE_REF];
	static	int	nxmin, nymin;
	static	unsigned int nLuminanceMax[_MAX_NUMBER_LUMINANCE_REF] = {0, 0};	// Variable to store max luminance value.
	static	unsigned int	nLuminanceMin = 127;	// Variable to store min luminance value.
	int		unsigned	nLuminance;
	static	int nCounter = 0;

	static  int nXMoment = 0;
	static  int nXCount = 0;
	static  int nYMoment = 0;
	static  int nYCount = 0;

	int nIndex;

	nTimer--;					// Decrement timer.
	
	if (nTimer == 0)
	{
		switch (nState)
		{
			case 0: // State 0 - Initialization and check if camera is ready.			
				nState = 1;
				nTimer = 1*__NUM_SYSTEMTICK_MSEC;
			break;

			case 1: // State 1 - Wait until a new frame is acquired in the image buffer before start processing.
				gnCameraLED = 1;					// Set camera LED intensity to low.
				nXindex = 0;							// Initialize all parameters.
				nYindex = 1;							// 13 Dec 2016: Start with line 2 of the image.  Line 1 of the
				// image is all black (luminance = 0), not sure why.
				// 8 Feb 2018: Image contains some error as stated, probably due to the TCM8230 camera.  So we
				// start with line 2.  Because of this if the actual image is quite bright, then there would be
				// no pixel with luminance = 0, and there would be no valid minimum luminance coordinate.  Thus
				// we can inform the external controller connected to the MVM that no valid minimum luminance is
				// detected.
				// The above restriction can be removed in future if a better camera module is used.
				
				nxmax[0] = gnImageWidth>>1;				// At the start of each frame initialized all (x,y) coordinates
				nymax[0] = gnImageHeight>>1;			// for maximum and minimum intensity to the center of the frame.
				nxmax[1] = gnImageWidth>>1;
				nymax[1] = gnImageHeight>>1;
				nxmin = gnImageWidth>>1;
				nymin = gnImageHeight>>1;				
				nCounter = 0;
				//PIN_FLAG3_SET;
				
				for (nIndex = 127; nIndex > 0; nIndex--)	// Search for the maximum luminance value in the luminance
				// histogram of the current frame.  Start from the largest
				{											// index, max luminance is assumed to be +127.
					if (gunIHisto[nIndex] > 3)				// At least 4 or more pixels having this luminance to qualify as a
					// valid maximum.
					// Note: 10 April 2018 - I discover that if we set to > 0, e.g. just
					// 1 pixel having a luminance value, the output of this process fluctuates
					// wildly.  It could be due to noise or some unknown error in the
					// camera sensor elements that cause a wrong luminance value to be
					// registered.  Thus setting a higher threshold eliminates this
					// effect, leading to a stable output as long as the scene captured
					// is the same.  The threshold can be adjusted based on
					// image resolution and camera type and surrounding EMI (electromagnetic
					// interference) level.
					{
						nLuminanceMax[nCounter] = nIndex;	// The first instance a non-zero location is found, this
						// represents the largest luminance.
						nCounter++;							// Point to next largest luminance.
						if (nCounter == _MAX_NUMBER_LUMINANCE_REF)
						// Here nLuminanceMax[0] stores the max luminance value,
						{									// nLuminanceMax[1] stores the next largest luminance value
							// and so forth.
							nIndex = 0;						// Once reach max. number of items, break the for-loop.
							nCounter = 0;					// Reset counter.
						}
					}
				}
				for (nIndex = 0; nIndex < 127; nIndex++)	// Search for the minimum luminance value in the luminance
				// histogram of the current frame.  Start from the smallest
				{											// index.
					if (gunIHisto[nIndex] > 3)				// At least 4 or more pixels having this luminance to qualify as a
					// valid minimum.  See comments above.
					{
						nLuminanceMin = nIndex;				// The first instance a non-zero location is found, this
						// represents the minimum luminance.
						nIndex = 1000;						// Break the for-loop.
					}
				}
				
				//OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
				nState = 2;
				nTimer = 1;
				nXMoment = 0;
				nXCount = 0;
				nYMoment = 0;
				nYCount = 0;
			break;

			case 2: // State 2 - Find the brightest and dimmest spots in the frame.
			//		Here we scan 2 lines of pixel data at one go, and do this multiple times until we cover the whole
			//		image frame.  This is to ensure that we don't hog the processor.  All processes should complete
			//		within 1 system tick.
			
			for (nXindex = 0; nXindex < gnImageWidth; nXindex++)					// Scan 1st row of image frame relative to nYindex.
			{
				nLuminance = gunImgAtt[nXindex][nYindex] & _LUMINANCE_MASK;			// Extract the 7-bits luminance value.
				if (nLuminance == nLuminanceMax[0])									// Search for highest luminance pixel location.
				{
					nXMoment = nXMoment + nXindex;									// Sum the x and y coordinates of all pixels whose luminance
					nXCount++;														// corresponds to the maximum luminance.  As we do this
					nYMoment = nYMoment + nYindex;									// we also keep track of the number of pixels.
					nYCount++;
				}
				else if (nLuminance == nLuminanceMax[1])							// Search for next highest luminance pixel
				{																	// location and assign to register.
					nxmax[1] = nXindex;
					nymax[1] = nYindex;
				}
				if (nLuminance == nLuminanceMin)									// Search for lowest luminance pixel location.
				{
					nxmin = nXindex;
					nymin = nYindex;
				}
			}
			
			nYindex++;
			if (nYindex == gnImageHeight)											// Is this the last row?
			{
				//OSSetTaskContext(ptrTask, 3, 1);									// Next state = 3, timer = 1.
				nState = 3;
				nTimer = 1;
				break;																// Exit current state.
			}
			
			for (nXindex = 0; nXindex < gnImageWidth; nXindex++)					// Scan 2nd row of image frame.
			{
				nLuminance = gunImgAtt[nXindex][nYindex] & _LUMINANCE_MASK;			// Extract the 7-bits luminance value.
				if (nLuminance == nLuminanceMax[0])									// Search for highest luminance pixel location.
				{
					nXMoment = nXMoment + nXindex;									// Sum the x and y coordinates of all pixels whose luminance
					nXCount++;														// corresponds to the maximum luminance.  As we do this
					nYMoment = nYMoment + nYindex;									// we also keep track of the number of pixels.
					nYCount++;
				}
				else if (nLuminance == nLuminanceMax[1])							// Search for next highest luminance pixel
				{																	// location.
					nxmax[1] = nXindex;
					nymax[1] = nYindex;
				}
				if (nLuminance == nLuminanceMin)									// Search for lowest luminance pixel location.
				{
					nxmin = nXindex;
					nymin = nYindex;
				}
			}
			
			nYindex++;
			if (nYindex == gnImageHeight)											// Is it last row?
			{																		// Yes
				//OSSetTaskContext(ptrTask, 3, 1);									// Next state = 3, timer = 1.
				nState = 3;
				nTimer = 1;
			}
			else																	// Not yet last row.
			{
				//OSSetTaskContext(ptrTask, 2, 1);									// Next state = 2, timer = 1.
				nState = 2;
				nTimer = 1;
			}
			break;
			
			case 3: // State 3 - Add marker 1 to the external display to highlight the result, also find peak-to-average value.
			nxmax[0] = nXMoment / nXCount;											// Compute the average location of the brightest pixels.
			nymax[0] = nYMoment / nYCount;
			gobjRec1.nHeight = 5 ;													// Enable a square marker to be displayed in remote monitor
			gobjRec1.nWidth = 5 ;													// software.
			gobjRec1.nX = nxmax[0];													// Set the location of the marker to the average location
			gobjRec1.nY = nymax[0];													// of the brightest pixels.
			gobjRec1.nColor = 3;													// Set marker color to blue (see the source codes for remote monitor)
			
			gobjRec2.nHeight = 5 ;													// Enable a square marker to be displayed in remote monitor
			gobjRec2.nWidth = 5 ;													// software.
			gobjRec2.nX = nxmin;													// Set the location of the marker to the first location
			gobjRec2.nY = nymin;													// of the dimmest pixel.
			gobjRec2.nColor = 2;													// Set marker color to light green.
			
			gunMaxLuminance = nLuminanceMax[0];										// Peak luminance value for this frame.
			gunPtoALuminance = gunMaxLuminance - gunAverageLuminance;				// Peak-to-Average luminance for this frame.

			//OSSetTaskContext(ptrTask, 4, 1);										// Next state = 4, timer = 1.
			nState = 4;
			nTimer = 1;
			break;
			
			case 4: // State 4 - Transmit status to external controller (4 bytes).
				gbytTXbuffer2[0] = 1;												// Load data, process ID.
				gbytTXbuffer2[1] = gunMaxLuminance;									// Peak luminance value for this frame.
				gbytTXbuffer2[2] = nxmax[0];
				gbytTXbuffer2[3] = nymax[0];
				//OSSetTaskContext(ptrTask, 5, 1*__NUM_SYSTEMTICK_MSEC);     // Next state = 5, timer = 1 msec.
				nState = 5;
				nTimer = 1;
			break;


			case 5:	// State 5 - Check if process ID has changed.
			gnImageProcessingAlgorithmBusy = _IMAGEPROCESSINGALGORITHM_IDLE;
			//PIN_FLAG3_CLEAR;
			//OSSetTaskContext(ptrTask, 1, 1);	// Next state = 1, timer = 1.
			nState = 1;
			nTimer = 1;
			break;

			default:
			//OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
			nState = 0;
			nTimer = 1;
			break;
		}
	}
}