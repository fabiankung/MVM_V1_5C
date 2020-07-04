//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT) 
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: User_Task.c
// Author(s)		: Fabian Kung
// Last modified	: 10 June 2020
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.4.0
//                    SAMS70_DFP 2.3.88

#include "./SAMS70_Drivers_BSP/osmain.h"
#include "./SAMS70_Drivers_BSP/Driver_UART2_V100.h"
#include "./SAMS70_Drivers_BSP/Driver_USART0_V100.h"
#include "./SAMS70_Drivers_BSP/Driver_TCM8230.h"

#include "CNN.h"

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
unsigned int	gunMaxLuminance;		// Maximum luminance in current frame, for debug purpose.
unsigned int	gunMinLuminance;		// Minimum luminance in current frame, for debug purpose.

int		gnSendSecondaryInfo = 0;		// Set to 1 (in Image Processing Tasks) to transmit secondary info such as marker location, 
										// size attribute and string to remote display.  Else set to 0.  If this is set 
										// to 1, after the transmission of a line of pixel data, the secondary info
										// packet will be send, and this will be clear to 0 by Proce_MessageLoop_StreamImage().
										// In present implementation the Proce_MessageLoop_StreamImage() will automatically 
										// set this to 1 after transmitting a frame to send secondary info automatically.
int		gnImageProcessingAlgorithm = 1;	// Current active image processing algorithm ID. 
										
unsigned int	gunIPResult[_IMAGE_HRESOLUTION/4][_IMAGE_VRESOLUTION];

int gnDebug;
int gnDebug2;

// --- Local function prototype ---
void	SetIPResultBuffer(int , int , unsigned int);	// Function to update the bytes in gunIPResult.
int		nConv2D(int, int, int *, int);
int     nMaxPool2D(int, int, int, int);

#define     _DEVICE_RESET               0x00
#define     _RES_PARAM                  0x01
#define     _SET_PARAM                  0x02

#define PIN_HC_05_RESET_CLEAR   PIOD->PIO_ODSR |= PIO_ODSR_P24                   // Pin PD24 = Reset pin for HC-05 bluetooth module.
#define PIN_HC_05_RESET_SET		PIOD->PIO_ODSR &= ~PIO_ODSR_P24					 // Active low.

#define PIN_FLAG1_SET			PIOD->PIO_ODSR |= PIO_ODSR_P21;					// Set flag 1.
#define PIN_FLAG1_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P21;				// Clear flag 1.
#define PIN_FLAG2_SET			PIOD->PIO_ODSR |= PIO_ODSR_P22;					// Set flag 2.
#define PIN_FLAG2_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P22;				// Clear flag 2.
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
/// Last modified	: 22 Nov 2019
///
/// Code Version	: 0.991
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			: Pin PD24 = Reset pin for HC-05 BlueTooth module.
///
/// MODULES			: UART0 (Internal), PDC (Internal), USART0 (Internal).
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
/// 1. Message clearing loop for instructions from external controller via USART0 port at 38.4 Kbps.
/// 2. Message clearing loop and stream video image captured by the camera via UART0 port at 115.2 Kbps.
///
/// --- (1) Message clearing ---
/// Basically this process waits for 1-byte command from external controller (if connected). 
/// The commands are as follows:
/// Command:		Action:
/// [CMD]	        1 to 3 - Set camera LED to different intensity levels.
///                 else - Off camera LED.
///
/// --- (2) Stream video image ---
/// Stream the image captured by camera to remote display via UART port.  Note that the UART0 port
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
	int nTemp;
	int nIndex;
	
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
			
			OSSetTaskContext(ptrTask, 10, 1000*__NUM_SYSTEMTICK_MSEC);     // Next state = 10, timer = 1000 msec.
			break;

			case 1: // State 1 - Message clearing for UART0 and USART0. 
					// a) Message clearing for USART0, and 
					// b) wait for start signal from remote host on UART0.
					// The start signal is the character 'L' (for luminance data using RGB)
					// 'R' (for luminance data using R only)
					// 'G' (for luminance data using G only)
					// 'B' (for luminance data using B only)
					// 'G' (for gradient data).
					// 'H' (for hue data)
					// 'P' (for image processing buffer result, if available)
			
			// --- Message clearing for USART0 ---
			
			if (gbytRXbufptr2 > 0)	// Check if USART0 receive any 1 byte of data.
			{
				if (gSCIstatus2.bRXOVF == 0)		// Make sure no overflow error.
				{
					PIN_LED2_CLEAR;
					switch (gbytRXbuffer2[0])		// Decode command for MVM.
					{
						case 1:	// Set camera LED to lowest intensity.
							gnCameraLED = 1;
						break;
						
						case 2:	// Set camera LED to medium intensity.
							gnCameraLED = 3;
						break;
						
						case 3: // Set camera LED to highest intensity.
							gnCameraLED = 51;
						break;
						
						default:
							gnCameraLED = 0;	// Turn off camera LED.
						break;
					}
				}
				else
				{
					gSCIstatus2.bRXOVF = 0; 	// Reset overflow error flag.
				}
				gSCIstatus2.bRXRDY = 0;			// Reset valid data flag.
				gbytRXbufptr2 = 0; 				// Reset pointer.
				PIN_LED2_CLEAR;					// Turn off indicator LED2.
			}
			
			
			// --- Message clearing and stream video image for UART0 ---
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
			
			if (gSCIstatus.bTXRDY == 0)					// Check if  UART port is not busy.
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
					{									// to between 0-120, e.g. Hue/3, or between 0-90, e.g. Hue/4
														// so that the value can fit into 7-bits byte.
						//nTemp = gunImgAtt[0][nLineCounter] & _HUE_MASK;
						//nCurrentPixelData = nTemp >> (_HUE_SHIFT + 2);						
						nTemp = gunImgAtt[0][nLineCounter] & _HUE_MASK;		// Divide by 3.
						nCurrentPixelData = nTemp >> (_HUE_SHIFT);		
						nCurrentPixelData = nCurrentPixelData/3;					
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
					{													// to between 0-120, e.g. Hue/3, or between 0-90, e.g. Hue/4
						//so that the value can fit into 7-bits byte.
						//nTemp = gunImgAtt[nIndex][nLineCounter] & _HUE_MASK;	// Divide by 4.
						//nCurrentPixelData = nTemp >> (_HUE_SHIFT + 2);
						nTemp = gunImgAtt[nIndex][nLineCounter] & _HUE_MASK;	// Divide by 3.
						nCurrentPixelData = nTemp >> _HUE_SHIFT;
						nCurrentPixelData = nCurrentPixelData/3;
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
				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nXposCounter+3);		// Set number of bytes to transmit + 3 bytes (payload + header).
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
			//gnCameraLED = 0;
			
			gbytTXbuffer[0] = 0xFF;					// Start of line code.
			gbytTXbuffer[1] = 254;					// Line number of 254 indicate auxiliary data.
			gbytTXbuffer[2] = 20;					// Payload length is 20 bytes.
			
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
			gbytTXbuffer[13] = gnDebug;				// Debug byte 1, some tag along parameter for debugging purpose.
			gbytTXbuffer[14] = 0;					// Not used for now.
			gbytTXbuffer[15] = 0;
			//gbytTXbuffer[16] = 0;
			//gbytTXbuffer[17] = 0;
			//gbytTXbuffer[18] = 0;
			gbytTXbuffer[16] = gnDebug>>8;			// Debug byte 2.
			gbytTXbuffer[17] = gnDebug>>16;			// Debug byte 3.
			gbytTXbuffer[18] = gnDebug>>24;			// Debug byte 4.
			gbytTXbuffer[19] = gnDebug2;			// Debug byte2 1.
			gbytTXbuffer[20] = gnDebug2>>8;			// Debug byte2 2.
			gbytTXbuffer[21] = gnDebug2>>16;		// Debug byte2 3.
			gbytTXbuffer[22] = gnDebug2>>24;		// Debug byte2 4.

 			SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
 									// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
 									// from the cache, it may not contains the up-to-date and correct data.  
 			XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
 			XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(23);		// Set number of bytes to transmit = 23 bytes including payload.
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
			
			case 10: // State 10 - Reset HC-05 BlueTooth module (if attached).  Note that if we keep the HC-05 module in
			// reset state, it will consume little power.  This trick can be used when we wish to power down
			// HC-05 to conserve power.
			PIN_HC_05_RESET_SET;					// Reset BlueTooth module.
			OSSetTaskContext(ptrTask, 11, 10*__NUM_SYSTEMTICK_MSEC);     // Next state = 11, timer = 10 msec.
			break;
			
			case 11: // State 11 - Reset HC-05 BlueTooth module.
			PIN_HC_05_RESET_CLEAR;					// Clear Reset to BlueTooth module.
			OSSetTaskContext(ptrTask, 1, 10*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 10 msec.
			break;
			
			default:
			OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}

///  Macro for 32-bits signed integer multiply and accumulate in assembly.
///  result = (op1*op2) + op3
__STATIC_FORCEINLINE uint32_t __MLAD (uint32_t op1, uint32_t op2, uint32_t op3)
{
	uint32_t result;

	__ASM volatile ("mla %0, %1, %2, %3" : "=r" (result) : "r" (op1), "r" (op2), "r" (op3) );
	return(result);
}


///
/// Function name	: Proce_Image4
///
/// Author			: Fabian Kung
///
/// Last modified	: 10 June 2020
///
/// Code Version	: 0.93
///
/// Processor		: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS			: None
///
/// MODULES			: Camera driver.
///					  Proce_USART0_Driver
///
/// RTOS			: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    :

#ifdef __OS_VER			// Check RTOS version compatibility.
#if __OS_VER < 1
#error "Proce_Image1: Incompatible OS version"
#endif
#else
#error "Proce_Image1: An RTOS is required with this function"
#endif

///
/// Description	:
/// An experimental image processing task to test the feasibility of running a convolution neural network process on 
/// this processor.
/// The structure of the CNN is as follows:
/*
model = tf.keras.models.Sequential([
tf.keras.layers.Conv2D(_layer0_channel, (3,3), strides = 2, activation='relu', input_shape=(_roi_height, _roi_width, 1)),
tf.keras.layers.MaxPooling2D(2, 2),
tf.keras.layers.Flatten(),
tf.keras.layers.Dense(_DNN1_node, activation='relu'),
tf.keras.layers.Dense(_DNN2_node, activation='softmax')
])
*/
/// _roi_height and _roi_width sets the size of the image (roi == Region of Interest).
/// The number of output currently supported is 4.

void Proce_Image4(TASK_ATTRIBUTE *ptrTask)
{
	static	int	nCurrentFrame;
	
	
	// Number of nodes in dense neural network (DNN) layer 1
	#define		__FLATTENNODE			__LAYER0_CHANNEL*(__LAYER0_X/2)*(__LAYER0_Y/2)
											
	#define		__LIMIT_CNN_FLATTEN		544	// Integer coefficients for DNN1 layer,  no Sobel pre-processing of luminance data in driver.
											// Frame rate (fps)        ROI Area (pixels)
											// 20.83                    50x70       	
											 
											// This value sets the max number of path or weight to compute for a node
											// in a DNN layer. It depends on the bandwidth of the MCU and other tasks  
											// and is determined experimentally. At present this is the only user task,
											// apart from the streaming routine. If more than user tasks are added, 
											// then this limit needs to be reduced further. As an example, suppose
											// we have Layer N with 1000 nodes map to Layer N+1 with 25 nodes in a
											// DNN structure.  To compute the value in node k in Layer N+1, we need
											// to: 
											// (1) Multiply the output of 1000 nodes in Layer N with the respective
											// weights, 
											// (2) Accumulate the results and add a bias, then
											// (3) Apply an activation function in node k in Layer N+1.  
											// Step (1) is the most time consuming and to limit this operating within
											// one Systick, we cap the maximum number of multiply-and-accumulate
											// operation.  Thus for this example, in the 1st cycle the system will
											// perform multiply-and-accumulate for 1st 864 outputs from Layer N, then
											// the 2nd cycle will perform subsequent 126 multiply-and-accumulate 
											// operations.  In other words Step (1) will require 2 Systicks to complete.
											// A smaller __LIMIT_CNN_FLATTEN would entails more Systicks cycle.
	static	int ni, nj;
	static	int ni2, nj2, nfilter;
	static	int	nFilA[9];
	static  int nBias;
	int		nConvRes1[__LAYER0_X];
	int		nConvRes2[__LAYER0_X];
	int		nTemp;
	
	static  int	nROI_Startx, nROI_Starty, nROI_Stopx, nROI_Stopy;
	static  int	nResFlat[__FLATTENNODE];	// 1D array to store the flatten nodes for Fully Connected Network.
	static  int nResFlatOffset;
   
	static	int nNode;
	static  int nStartIndex;
	static  int nStopIndex;
	static	int64_t lnsTemp;
	int64_t	lnTemp2;
	int64_t	lnBias;
	static  int nDNN1Out[__DNN1NODE];		// Nodes for dense layer 1.
	static  int nDNN2Out[__DNN2NODE];		// Nodes for dense layer 2.
	static  int nObjectPresent;
    static  int nCompCount;
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization and check if camera is ready.
			if (gnCameraReady == _CAMERA_READY)
			{
				gnCameraLED = 1;					// Set camera LED intensity to low.
				nObjectPresent = 0;
				nCurrentFrame = gnFrameCounter;		// Set frame counter to be same as current frame.
				OSSetTaskContext(ptrTask, 1, 10*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 10 msec.
			}
			else
			{
				OSSetTaskContext(ptrTask, 0, 1*__NUM_SYSTEMTICK_MSEC);     // Next state = 0, timer = 1 msec.
			}
			break;

			case 1: // State 1 - Wait until a new frame is acquired in the image buffer before start processing.
			if (gnFrameCounter != nCurrentFrame)			// Check if new image frame has been captured.
			{
				PIN_FLAG4_SET;								// Set debug flag.	
				nCurrentFrame = gnFrameCounter;				// Update current frame counter.
				// Set up the variables controlling the region of the image to analyze.
				nROI_Startx = __ROI_STARTX;
				nROI_Stopx = __ROI_STARTX + __ROI_WIDTH;
				nROI_Starty = __ROI_STARTY;
				nROI_Stopy = __ROI_STARTY + __ROI_HEIGHT;	
				nfilter = 0;								// Index to filter. Start with filter 0.
				nNode = 0;									// Index to DNN node. Start with node 0.	
				//OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
				
				nCompCount = 0;
				
				nFilA[0] = gnL1f[nfilter][0][0];			// Get the coefficients of the filter matrix and bias.
				nFilA[1] = gnL1f[nfilter][0][1];			// Note that the coefficients are normalized to integer
				nFilA[2] = gnL1f[nfilter][0][2];			// between -1,000,000 to +1,000,000.
				nFilA[3] = gnL1f[nfilter][1][0];
				nFilA[4] = gnL1f[nfilter][1][1];
				nFilA[5] = gnL1f[nfilter][1][2];
				nFilA[6] = gnL1f[nfilter][2][0];
				nFilA[7] = gnL1f[nfilter][2][1];
				nFilA[8] = gnL1f[nfilter][2][2];
				nBias = gnL1fbias[nfilter];
				nResFlatOffset = 0;							// Initialize offset index for result of flatten array.
				nj = nROI_Starty;							// Point to 1st row in ROI.	
				PIN_FLAG4_CLEAR;							// Clear debug flag.	
				OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.		
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);			// Next state = 1, timer = 1.
			}
			break;
			
			/*
			case 2: // State 2 - Initialize Layer 0 filter coefficients array and range parameters.		
			PIN_FLAG4_SET;									// Set debug flag.				
			nFilA[0] = gnL1f[nfilter][0][0];				// Get the coefficients of the filter matrix and bias.
			nFilA[1] = gnL1f[nfilter][0][1];				// Note that the coefficients are normalized to integer 
			nFilA[2] = gnL1f[nfilter][0][2];				// between -1,000,000 to +1,000,000.
			nFilA[3] = gnL1f[nfilter][1][0];
			nFilA[4] = gnL1f[nfilter][1][1];
			nFilA[5] = gnL1f[nfilter][1][2];
			nFilA[6] = gnL1f[nfilter][2][0];
			nFilA[7] = gnL1f[nfilter][2][1];
			nFilA[8] = gnL1f[nfilter][2][2];
			nBias = gnL1fbias[nfilter];
			nResFlatOffset = 0;								// Initialize offset index for result of flatten array.
			nj = nROI_Starty;								// Point to 1st row in ROI.			
			PIN_FLAG4_CLEAR;								// Clear debug flag.
			OSSetTaskContext(ptrTask,3, 1);					// Next state = 3, timer = 1.
			break;
			*/

			case 3: // State 3 - Compute convolution operation for Layer 0 along horizontal direction for
			        // two times, followed by max pooling operation.  
			PIN_FLAG4_SET;									// Set debug flag.
			// 1st Cycle.
			ni2 = 0;										// Reset index to store temporary results from
															// 2D convolution operation.
			// --- Conv2D operation on two consecutive rows ---
			for (ni = nROI_Startx; ni < nROI_Stopx - (__FILTER_SIZE-1); ni = ni + __FILTER_STRIDE)
			{				
				nConvRes1[ni2] = nConv2D(ni,nj, nFilA, nBias);						// Row 1.
				nConvRes2[ni2] = nConv2D(ni,nj + __FILTER_STRIDE, nFilA, nBias);	// Row 2.
				ni2++;
			}	
			
			// --- Max-pooling operation ---
			nj2 = 0;
			for (ni2 = 0; ni2 < __LAYER0_X-1; ni2 = ni2 + 2)
			{			
				nResFlat[(nj2 + nResFlatOffset)*__LAYER0_CHANNEL + nfilter] = nMaxPool2D(nConvRes1[ni2], nConvRes1[ni2+1], nConvRes2[ni2], nConvRes2[ni2+1]);
				nj2++;									// Next element in the flatten array.
			}
			nResFlatOffset = nResFlatOffset + nj2;		// Update offset index for result of flatten array.					
			nj = nj + __FILTER_STRIDE;					// Advance the vertical index 2x the stride.
			nj = nj + __FILTER_STRIDE;					// distance.
										
			if (nj < (nROI_Stopy-4))					// At least 5 rows before last line as we process
														// 5 rows of pixels at a time for stride = 2.
			{
				//OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.
			}
			else
			{												// Completed the ROI.
				PIN_FLAG4_CLEAR;							// Clear debug flag.
				nfilter++;									// Next filter.
				if (nfilter == __LAYER0_CHANNEL)			// Check if all convolution filters are attended to.
				{
					lnsTemp = 0;							// Clear the 64-bits accumulator register first.
					ni = 0;									// Initialize index to point to 1st output of Flatten layer. 
					OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.
				}
				else
				{
					//OSSetTaskContext(ptrTask, 2, 1);		// Next state = 2, timer = 1.
					
					nFilA[0] = gnL1f[nfilter][0][0];		// Get the coefficients of the filter matrix and bias.
					nFilA[1] = gnL1f[nfilter][0][1];		// Note that the coefficients are normalized to integer
					nFilA[2] = gnL1f[nfilter][0][2];		// between -1,000,000 to +1,000,000.
					nFilA[3] = gnL1f[nfilter][1][0];
					nFilA[4] = gnL1f[nfilter][1][1];
					nFilA[5] = gnL1f[nfilter][1][2];
					nFilA[6] = gnL1f[nfilter][2][0];
					nFilA[7] = gnL1f[nfilter][2][1];
					nFilA[8] = gnL1f[nfilter][2][2];
					nBias = gnL1fbias[nfilter];
					nResFlatOffset = 0;						// Initialize offset index for result of flatten array.
					nj = nROI_Starty;						// Point to 1st row in ROI.		
					OSSetTaskContext(ptrTask, 3, 1);		// Next state = 3, timer = 1.			
				}
				break;										// Quit this process.
			}
			
			// 2nd Cycle.
			ni2 = 0;										// Reset index to store temporary results from
															// 2D convolution operation.
			// --- Conv2D operation on two consecutive rows ---
			for (ni = nROI_Startx; ni < nROI_Stopx - (__FILTER_SIZE-1); ni = ni + __FILTER_STRIDE)
			{
				nConvRes1[ni2] = nConv2D(ni,nj, nFilA, nBias);						// Row 1.
				nConvRes2[ni2] = nConv2D(ni,nj + __FILTER_STRIDE, nFilA, nBias);	// Row 2.
				ni2++;
			}
			
			// --- Max-pooling operation ---
			nj2 = 0;
			for (ni2 = 0; ni2 < __LAYER0_X-1; ni2 = ni2 + 2)
			{
				nResFlat[(nj2 + nResFlatOffset)*__LAYER0_CHANNEL + nfilter] = nMaxPool2D(nConvRes1[ni2], nConvRes1[ni2+1], nConvRes2[ni2], nConvRes2[ni2+1]);
				nj2++;									// Next element in the flatten array.
			}
			nResFlatOffset = nResFlatOffset + nj2;		// Update offset index for result of flatten array.		
			nj = nj + __FILTER_STRIDE;					// Advance the vertical index 2x the stride.
			nj = nj + __FILTER_STRIDE;					// distance.
			
			if (nj < (nROI_Stopy-4))					// At least 5 rows before last line as we process
			// 5 rows of pixels at a time for stride = 2.
			{
				//OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.
			}
			else
			{												// Completed the ROI.
				PIN_FLAG4_CLEAR;							// Clear debug flag.
				nfilter++;									// Next filter.
				if (nfilter == __LAYER0_CHANNEL)			// Check if all convolution filters are attended to.
				{
					lnsTemp = 0;							// Clear the 64-bits accumulator register first.
					ni = 0;									// Initialize index to point to 1st output of Flatten layer. 
					OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.
				}
				else
				{
					//OSSetTaskContext(ptrTask, 2, 1);		// Next state = 2, timer = 1.

					nFilA[0] = gnL1f[nfilter][0][0];		// Get the coefficients of the filter matrix and bias.
					nFilA[1] = gnL1f[nfilter][0][1];		// Note that the coefficients are normalized to integer
					nFilA[2] = gnL1f[nfilter][0][2];		// between -1,000,000 to +1,000,000.
					nFilA[3] = gnL1f[nfilter][1][0];
					nFilA[4] = gnL1f[nfilter][1][1];
					nFilA[5] = gnL1f[nfilter][1][2];
					nFilA[6] = gnL1f[nfilter][2][0];
					nFilA[7] = gnL1f[nfilter][2][1];
					nFilA[8] = gnL1f[nfilter][2][2];
					nBias = gnL1fbias[nfilter];
					nResFlatOffset = 0;						// Initialize offset index for result of flatten array.
					nj = nROI_Starty;						// Point to 1st row in ROI.
					OSSetTaskContext(ptrTask, 3, 1);		// Next state = 3, timer = 1.					
				}
				break;										// Quit this process.
			}			

			// 3rd Cycle.
			ni2 = 0;										// Reset index to store temporary results from
			// 2D convolution operation.
			// --- Conv2D operation on two consecutive rows ---
			for (ni = nROI_Startx; ni < nROI_Stopx - (__FILTER_SIZE-1); ni = ni + __FILTER_STRIDE)
			{
				nConvRes1[ni2] = nConv2D(ni,nj, nFilA, nBias);						// Row 1.
				nConvRes2[ni2] = nConv2D(ni,nj + __FILTER_STRIDE, nFilA, nBias);	// Row 2.
				ni2++;
			}
			
			// --- Max-pooling operation ---
			nj2 = 0;
			for (ni2 = 0; ni2 < __LAYER0_X-1; ni2 = ni2 + 2)
			{
				nResFlat[(nj2 + nResFlatOffset)*__LAYER0_CHANNEL + nfilter] = nMaxPool2D(nConvRes1[ni2], nConvRes1[ni2+1], nConvRes2[ni2], nConvRes2[ni2+1]);
				nj2++;									// Next element in the flatten array.
			}
			nResFlatOffset = nResFlatOffset + nj2;		// Update offset index for result of flatten array.
			nj = nj + __FILTER_STRIDE;					// Advance the vertical index 2x the stride.
			nj = nj + __FILTER_STRIDE;					// distance.
			
			if (nj < (nROI_Stopy-4))					// At least 5 rows before last line as we process
			// 5 rows of pixels at a time for stride = 2.
			{
				//OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.
			}
			else
			{												// Completed the ROI.
				PIN_FLAG4_CLEAR;							// Clear debug flag.
				nfilter++;									// Next filter.
				if (nfilter == __LAYER0_CHANNEL)			// Check if all convolution filters are attended to.
				{
					lnsTemp = 0;							// Clear the 64-bits accumulator register first.
					ni = 0;									// Initialize index to point to 1st output of Flatten layer.
					OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.
				}
				else
				{
					//OSSetTaskContext(ptrTask, 2, 1);		// Next state = 2, timer = 1.

					nFilA[0] = gnL1f[nfilter][0][0];		// Get the coefficients of the filter matrix and bias.
					nFilA[1] = gnL1f[nfilter][0][1];		// Note that the coefficients are normalized to integer
					nFilA[2] = gnL1f[nfilter][0][2];		// between -1,000,000 to +1,000,000.
					nFilA[3] = gnL1f[nfilter][1][0];
					nFilA[4] = gnL1f[nfilter][1][1];
					nFilA[5] = gnL1f[nfilter][1][2];
					nFilA[6] = gnL1f[nfilter][2][0];
					nFilA[7] = gnL1f[nfilter][2][1];
					nFilA[8] = gnL1f[nfilter][2][2];
					nBias = gnL1fbias[nfilter];
					nResFlatOffset = 0;						// Initialize offset index for result of flatten array.
					nj = nROI_Starty;						// Point to 1st row in ROI.
					OSSetTaskContext(ptrTask, 3, 1);		// Next state = 3, timer = 1.
				}
				break;										// Quit this process.
			}
			
			// 4th Cycle.
			ni2 = 0;										// Reset index to store temporary results from
															// 2D convolution operation.
			// --- Conv2D operation on two consecutive rows ---
			for (ni = nROI_Startx; ni < nROI_Stopx - (__FILTER_SIZE-1); ni = ni + __FILTER_STRIDE)
			{
				nConvRes1[ni2] = nConv2D(ni,nj, nFilA, nBias);						// Row 1.
				nConvRes2[ni2] = nConv2D(ni,nj + __FILTER_STRIDE, nFilA, nBias);	// Row 2.
				ni2++;
			}
			
			// --- Max-pooling operation ---
			nj2 = 0;
			for (ni2 = 0; ni2 < __LAYER0_X-1; ni2 = ni2 + 2)
			{
				nResFlat[(nj2 + nResFlatOffset)*__LAYER0_CHANNEL + nfilter] = nMaxPool2D(nConvRes1[ni2], nConvRes1[ni2+1], nConvRes2[ni2], nConvRes2[ni2+1]);
				nj2++;									// Next element in the flatten array.
			}
			nResFlatOffset = nResFlatOffset + nj2;		// Update offset index for result of flatten array.		
			nj = nj + __FILTER_STRIDE;					// Advance the vertical index 2x the stride.
			nj = nj + __FILTER_STRIDE;					// distance.										
										
			PIN_FLAG4_CLEAR;							// Clear debug flag.	
					
			if (nj < (nROI_Stopy-4))					// At least 5 rows before last line as we process
														// 5 rows of pixels at a time for stride = 2.
			{
				OSSetTaskContext(ptrTask, 3, 1);			// Next state = 3, timer = 1.
			}
			else
			{												// Completed the ROI.
				nfilter++;									// Next filter.
				if (nfilter == __LAYER0_CHANNEL)			// Check if all convolution filters are attended to.
				{
					lnsTemp = 0;							// Clear the 64-bits accumulator register first.
					ni = 0;									// Initialize index to point to 1st output of Flatten layer. 
					OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.
				}
				else
				{					
					//OSSetTaskContext(ptrTask, 2, 1);		// Next state = 2, timer = 1.

					nFilA[0] = gnL1f[nfilter][0][0];		// Get the coefficients of the filter matrix and bias.
					nFilA[1] = gnL1f[nfilter][0][1];		// Note that the coefficients are normalized to integer
					nFilA[2] = gnL1f[nfilter][0][2];		// between -1,000,000 to +1,000,000.
					nFilA[3] = gnL1f[nfilter][1][0];
					nFilA[4] = gnL1f[nfilter][1][1];
					nFilA[5] = gnL1f[nfilter][1][2];
					nFilA[6] = gnL1f[nfilter][2][0];
					nFilA[7] = gnL1f[nfilter][2][1];
					nFilA[8] = gnL1f[nfilter][2][2];
					nBias = gnL1fbias[nfilter];
					nResFlatOffset = 0;						// Initialize offset index for result of flatten array.
					nj = nROI_Starty;						// Point to 1st row in ROI.
					OSSetTaskContext(ptrTask, 3, 1);		// Next state = 3, timer = 1.
				}
			}						
			break;
			
			case 4: // State 4 - Compute the output of each nodes in Layer DNN1.
			PIN_FLAG4_SET;
			while (nCompCount < __LIMIT_CNN_FLATTEN)	// Check if reach the BW if one Systick.
			{
				lnTemp2 = nResFlat[ni];					// Load and convert to 64 bits integer.
				nTemp = gnDNN1w[ni][nNode];				// Load 32 bits integer coefficients.
				lnsTemp = lnsTemp + (lnTemp2*nTemp);	// Multiply and accumulate.	
														// 9 May 2020: Initially I use 64 bits integer to
														// store both multiplicands.  I discovered that if
														// one of the multiplicands is 32 bits integer, the
														// execution time will be halved. If both multiplicands
														// are 32-bits integer, then overflow occurs.							
				ni++;									// Increment index.
				nCompCount++;							// Increment computation counter.
				if (ni >= __FLATTENNODE)				// Check if reach end of flatten input for current DNN1 node.
				{
					lnBias = gnDNN1bias[nNode];			// Note: 19 May 2020. I discovered that because gnDNN1bias[]
														// is a 32-bits integer register, if we use
														// gnDNN1bias[nNode]*1000000, overflow will occur. Somehow
														// the processor stores the result in 32-bits register before
														// converting to 64-bits long integer.  This causes overflow.
					lnBias = lnBias*1000000;			// Thus we need to divide this into two steps.
					lnsTemp = lnsTemp + lnBias;			// Add bias.
					nDNN1Out[nNode] = lnsTemp/1000000;	// Scale back to 32 bits integer.
					// ReLu activation function
					if (nDNN1Out[nNode] < 0)
					{
						nDNN1Out[nNode] = 0;
					}					
					
					ni = 0;								// Reset index.
					lnsTemp = 0;						// Clear the 64-bits accumulator register first.					
					nNode++;							// Point to next node in layer.
					if (nNode >= __DNN1NODE)			// Check if output for all nodes in layer are computed.
					{
						break;
					}
				}
			}
			nCompCount = 0;								// Reset computation counter.
			if (nNode < __DNN1NODE)						// Check for end of nodes in DNN1.
			{		
				OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.	
			}
			else
			{
				OSSetTaskContext(ptrTask, 5, 1);		// Next state = 5, timer = 1.
			}
			PIN_FLAG4_CLEAR;
			break;
			
			/*
			case 40: // State 4 - Compute the output of each nodes in Layer DNN1.
			PIN_FLAG4_SET;	
													
			for (ni = nStartIndex; ni < nStopIndex; ni++)
			{
				lnTemp2 = nResFlat[ni];					// Load and convert to 64 bits integer.
				//nTemp = 1000000*gfDNN1w[ni][nNode];		// Load and convert to 32 bits integer.
				nTemp = gnDNN1w[ni][nNode];				// Load 32 bits integer coefficients.
				lnsTemp = lnsTemp + (lnTemp2*nTemp);	// Multiply and accumulate.
														// 9 May 2020: Initially I use 64 bits integer to
														// store both multiplicands.  I discovered that if
														// one of the multiplicands is 32 bits integer, the
														// execution time will be halved. If both multiplicands
														// are 32-bits integer, then overflow occurs.
			}			
			
			if (nStopIndex >= __FLATTENNODE)			// Check if reach end of flatten input for current DNN1 node.
			{
				//lnBias = gfDNN1bias[nNode]*1000000000000;
				lnBias = gnDNN1bias[nNode];					// Note: 19 May 2020. I discovered that because gnDNN1bias[] 
															// is a 32-bits integer register, if we use 
															// gnDNN1bias[nNode]*1000000, overflow will occur. Somehow
															// the processor stores the result in 32-bits register before
															// converting to 64-bits long integer.  This causes overflow. 
				lnBias = lnBias*1000000;					// Thus we need to divide this into two steps.
				lnsTemp = lnsTemp + lnBias;					// Add bias.
				nDNN1Out[nNode] = lnsTemp/1000000;			// Scale back to 32 bits integer.
				// ReLu activation function
				if (nDNN1Out[nNode] < 0)
				{
					nDNN1Out[nNode] = 0;
				}		
				
				nNode++;								// Point to next node in DNN1.
				if (nNode < __DNN1NODE)					// Check for end of nodes in DNN1.
				{
					lnsTemp = 0;						// Clear the 64-bits accumulator register first.
					nStartIndex = 0;					// Initialized start and stop index again
					if (__FLATTENNODE <= __LIMIT_CNN_FLATTEN)			// for next node in DNN1 layer.
					{
						nStopIndex = __FLATTENNODE;
					}
					else
					{
						nStopIndex = __LIMIT_CNN_FLATTEN;
					}					
					OSSetTaskContext(ptrTask, 4, 1);	// Next state = 4, timer = 1.
				}
				else                                    // All nodes in DNN1 computed. 
				{										// Computed all the node values in DNN1, next layer.
					nNode = 0;							// Reset node counter.
					OSSetTaskContext(ptrTask, 5, 1);	// Next state = 5, timer = 1.
				}					
			}
			else                                        // Not yet complete all the flatten paths for current DNN1 node. 
			{
				nStartIndex = nStopIndex;				// Update start index.
				nStopIndex = nStopIndex + __LIMIT_CNN_FLATTEN;			// Update stop index.
				if (nStopIndex > __FLATTENNODE)			// Limit stop index to last flatten index.
				{
					nStopIndex = __FLATTENNODE;
				}
				OSSetTaskContext(ptrTask, 4, 1);		// Next state = 4, timer = 1.
			}
			PIN_FLAG4_CLEAR;
			break;
			*/
			
			case 5: // State 5 - Compute the output of each nodes in Layer DNN2.
			PIN_FLAG4_SET;
			
			for (nNode = 0; nNode < __DNN2NODE; nNode++)
			{
				lnsTemp = 0;							// Clear temp 64-bits register first.
				for (ni = 0; ni < __DNN1NODE; ni++)
				{
					lnTemp2 = nDNN1Out[ni];				// Load and convert to 64 bits integer.
					//nTemp = 1000000*gfDNN2w[ni][nNode];	// Load and convert to 32 bits integer.
					nTemp = gnDNN2w[ni][nNode];				// Load 32 bits integer coefficient.
					lnsTemp = lnsTemp + (lnTemp2*nTemp);	// Multiply and accumulate.
				}
				//lnBias = gfDNN2bias[nNode]*1000000000000;
				lnBias = gnDNN2bias[nNode];
				lnBias = lnBias*1000000;
				lnsTemp = lnsTemp + lnBias;				// Add bias.
				nDNN2Out[nNode] = lnsTemp/1000000;		// Scale back to 32 bits integer.				
			}
			PIN_FLAG4_CLEAR;
			
			// Softmax output function
			// In softmax, the output of each node is converted to probability.  As the output value of each
			// node can be negative, taking the exponent of the value will convert it into a positive real
			// value.  The exponent is then normalized to a real value between 0.0 to 1.0 by dividing it to
			// the sum of all the exponent of each output node.  
			// Since exponent computation takes a lot of machine cycles, here we just search for the largest
			// value among the output of all output nodes.  Larger value correlated to higher probability.  
			// So the largest value will be chosen as the decision by the CNN.
			nTemp = nDNN2Out[0];			// Find the largest integer values from the output layer.
			nObjectPresent = 0;				// by searching through all outputs.
			if (nDNN2Out[1] > nTemp)
			{
				nTemp = nDNN2Out[1];
				nObjectPresent = 1;
			}
			if (nDNN2Out[2] > nTemp)
			{
				nTemp = nDNN2Out[2];
				nObjectPresent = 2;
			}
			if (nDNN2Out[3] > nTemp)
			{
				nObjectPresent = 3;
			}
			if (nDNN2Out[4] > nTemp)
			{
				nObjectPresent = 4;
			}			
			
			if (nObjectPresent > 0)				// If object is present, show a marker.  
			{
				gnCameraLED = 5;				// Set camera LED intensity to high intensity.
				gobjRec1.nColor = 2;			// Green color.
				gobjRec1.nHeight = 6;			// Enable a square marker to be displayed in remote monitor
												// software.
				gobjRec1.nY = __ROI_STARTY + (__ROI_HEIGHT/2) - 3;	
				if (nObjectPresent == 1)		// Left. The marker horizontal position 
												// corresponds to the output.
				{								// Put marker on left side of ROI.
					gobjRec1.nX = __ROI_STARTX + (__ROI_WIDTH/6) - 3;		
					gobjRec1.nWidth = 6;							
				}
				else if (nObjectPresent == 2)	// Right.
				{								// Put marker on right side of ROI.
					gobjRec1.nX = __ROI_STARTX + (5*__ROI_WIDTH/6) - 3;
					gobjRec1.nWidth = 6;	
				}
				else  if (nObjectPresent == 3)	// Front.
				{								// Put marker on middle of ROI.
					gobjRec1.nX = __ROI_STARTX + (3*__ROI_WIDTH/6) - 3;
					gobjRec1.nWidth = 6;	
				}				
				else  // nObjectPresent = 4.	// Blocked.
				{								// Put marker on middle of ROI.
					gobjRec1.nX = __ROI_STARTX + (3*__ROI_WIDTH/6) - 15;
					gobjRec1.nWidth = 30;						
				}		
			}
			else
			{									// If object is not present.
				nObjectPresent = 0;
				gnCameraLED = 1;				// Set camera LED intensity to low.
				gobjRec1.nX = 0;				// Set the location of the marker.
				gobjRec1.nY = 0;				
				gobjRec1.nHeight = 0 ;			// Disable square marker to be displayed in remote monitor
				gobjRec1.nWidth = 0 ;			// software.
			}
			
			// --- Debug routines ---
			//gnDebug = nDNN2Out[0];
			gnDebug2 = nDNN2Out[1];
			gnDebug = nObjectPresent;
			//gnDebug2 = nDNN1Out[3];
			//gnDebug = nDNN1Coeff[0];
			//gnDebug2 = nDNN1Coeff[7];
			//gnDebug = nResFlat[9];
			//gnDebug2 = nResFlat[10];
			//gnDebug = nConvRes1[11];
			//gnDebug2 = nConvRes2[11];			
			
			if (gSCIstatus2.bTXRDY == 0)		// Check if any data to send via UART.
			{
				gbytTXbuffer2[0] = 4;			// Load data, process ID.
				gbytTXbuffer2[1] = nObjectPresent;			
				gbytTXbuflen2 = 2;				// Set TX frame length.
				gSCIstatus2.bTXRDY = 1;			// Initiate TX.
				//OSSetTaskContext(ptrTask, 6, 1*__NUM_SYSTEMTICK_MSEC);      // Next state = 6, timer = 1 msec.	
				OSSetTaskContext(ptrTask, 6, 3);      // Next state = 6, timer = 4.	
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);	// Next state = 1, timer = 1.
			}				
			break;
			
			case 6: // State 6 - Continue to transfer the last 2 bytes of data to host controller.
			PIN_FLAG4_SET;
			if (gSCIstatus2.bTXRDY == 0)		// Check if any data to send via UART.
			{
				gbytTXbuffer2[0] = 0;			// 
				gbytTXbuffer2[1] = 0;			// 
				gbytTXbuflen2 = 2;				// Set TX frame length.
				gSCIstatus2.bTXRDY = 1;			// Initiate TX.
				OSSetTaskContext(ptrTask, 1, 1);      // Next state = 1, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 6, 1);	// Next state = 6, timer = 1.
			}
			PIN_FLAG4_CLEAR;
			//OSSetTaskContext(ptrTask, 1, 1);	// Next state = 1, timer = 1.
			break;

			default:
			OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
			break;
		}
	}
}

// Function to compute the 3x3 convolution operation on a small region 
// of the image buffer.
// ni, nj = (x,y) coordinate of start pixel in 3x3 image patch.
// nFilA = Address of array containing the 9 coefficients of the kernel or filter.
// nBias = Bias value.
__INLINE int	nConv2D(int ni, int nj, int * nFilA, int nBias)
{
	int	nLuminance[9];
	int nTemp;
	
	if (gnValidFrameBuffer == 1)					// Check frame buffer data valid flag.  If equals 1 means gunImgAtt2[] data
	{
		nLuminance[0] = gunImgAtt2[ni][nj] & _LUMINANCE_MASK; // Extract the 7-bits luminance value.
		nLuminance[1] = gunImgAtt2[ni+1][nj] & _LUMINANCE_MASK;
		nLuminance[2] = gunImgAtt2[ni+2][nj] & _LUMINANCE_MASK;
		nLuminance[3] = gunImgAtt2[ni][nj+1] & _LUMINANCE_MASK;
		nLuminance[4] = gunImgAtt2[ni+1][nj+1] & _LUMINANCE_MASK;
		nLuminance[5] = gunImgAtt2[ni+2][nj+1] & _LUMINANCE_MASK;
		nLuminance[6] = gunImgAtt2[ni][nj+2] & _LUMINANCE_MASK;
		nLuminance[7] = gunImgAtt2[ni+1][nj+2] & _LUMINANCE_MASK;
		nLuminance[8] = gunImgAtt2[ni+2][nj+2] & _LUMINANCE_MASK;
	}
	else
	{
		nLuminance[0] = gunImgAtt[ni][nj] & _LUMINANCE_MASK; // Extract the 7-bits luminance value.
		nLuminance[1] = gunImgAtt[ni+1][nj] & _LUMINANCE_MASK;
		nLuminance[2] = gunImgAtt[ni+2][nj] & _LUMINANCE_MASK;
		nLuminance[3] = gunImgAtt[ni][nj+1] & _LUMINANCE_MASK;
		nLuminance[4] = gunImgAtt[ni+1][nj+1] & _LUMINANCE_MASK;
		nLuminance[5] = gunImgAtt[ni+2][nj+1] & _LUMINANCE_MASK;
		nLuminance[6] = gunImgAtt[ni][nj+2] & _LUMINANCE_MASK;
		nLuminance[7] = gunImgAtt[ni+1][nj+2] & _LUMINANCE_MASK;
		nLuminance[8] = gunImgAtt[ni+2][nj+2] & _LUMINANCE_MASK;
	}
		
	// Convolution or cross-correlation operation with 3x3 filter. We try to avoid using for-loop to speed up the computation.
	// Note: 24 April 2020, I have tried a few approaches, using C codes without for-loop. Verified that this method is the
	// fastest, from a few tens of microseconds to a few microseconds! This method forces the compiler to use the 32-bits signed
	// integer multiply and accumulate assembly instruction of the Cortex M7 core, making it the most efficient.
				
	nTemp = (*nFilA)*(*nLuminance); 				// Correlation operation.
	nTemp = __MLAD(*(nFilA+1),*(nLuminance+1),nTemp);	// Using assembly multiply and accumulate instruction.
	nTemp = __MLAD(*(nFilA+2),*(nLuminance+2),nTemp);
	nTemp = __MLAD(*(nFilA+3),*(nLuminance+3),nTemp);
	nTemp = __MLAD(*(nFilA+4),*(nLuminance+4),nTemp);
	nTemp = __MLAD(*(nFilA+5),*(nLuminance+5),nTemp);
	nTemp = __MLAD(*(nFilA+6),*(nLuminance+6),nTemp);
	nTemp = __MLAD(*(nFilA+7),*(nLuminance+7),nTemp);
	nTemp = __MLAD(*(nFilA+8),*(nLuminance+8),nTemp);
	nTemp = (nTemp/128) + nBias;					// Add Bias term and normalized by 128. The luminance value
													// ranges from 0 to 127, during training of the CNN we normalize
													// by 128 to make it between 0 to 1.0.  So in inference we should
													// do the same.
	
	// ReLu activation function
	if (nTemp < 0)
	{
		nTemp = 0;
	}	
	return nTemp;
}

// Function to compute 2x2 max pooling result.
__INLINE int nMaxPool2D(int nData1, int nData2, int nData3, int nData4)
{
	int nTemp;
	
	// Look for largest value using brute force search method.
	nTemp = nData1;
	if (nData2 > nTemp)
	{
		nTemp = nData2;
	}
	if (nData3 > nTemp)
	{
		nTemp = nData3;
	}
	if (nData4 > nTemp)
	{
		nTemp = nData4;
	}	
	return nTemp;
}