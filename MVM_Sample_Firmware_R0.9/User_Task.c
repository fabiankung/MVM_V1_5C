//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT) 
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: User_Task.c
// Author(s)		: Fabian Kung
// Last modified	: 22 Nov 2019
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.0.1

#include "./SAMS70_Drivers_BSP/osmain.h"
#include "./SAMS70_Drivers_BSP/Driver_UART2_V100.h"
#include "./SAMS70_Drivers_BSP/Driver_USART0_V100.h"
#include "./SAMS70_Drivers_BSP/Driver_TCM8230.h"

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

// --- Local function prototype ---

#define     _DEVICE_RESET               0x00
#define     _RES_PARAM                  0x01
#define     _SET_PARAM                  0x02

#define PIN_HC_05_RESET_CLEAR   PIOD->PIO_ODSR |= PIO_ODSR_P24                   // Pin PD24 = Reset pin for HC-05 bluetooth module.
#define PIN_HC_05_RESET_SET		PIOD->PIO_ODSR &= ~PIO_ODSR_P24					 // Active low.

#define PIN_FLAG1_SET			PIOD->PIO_ODSR |= PIO_ODSR_P21;					// Set flag 1.
#define PIN_FLAG1_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P21;				// Clear flag 1.
#define PIN_FLAG2_SET			PIOD->PIO_ODSR |= PIO_ODSR_P22;					// Set flag 2.
#define PIN_FLAG2_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P22;				// Clear flag 2.

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
			//gnCameraLED = 0;
			
			gbytTXbuffer[0] = 0xFF;					// Start of line code.
			gbytTXbuffer[1] = 254;					// Line number of 254 indicate auxiliary data.
			gbytTXbuffer[2] = 11;					// Payload length is 11 bytes.
			
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
			gbytTXbuffer[13] = 0;					// Some tag along parameter for debugging purpose, not used for now.

 			SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
 									// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
 									// from the cache, it may not contains the up-to-date and correct data.  
 			XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
 			XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(14);		// Set number of bytes to transmit = 14 bytes including payload.
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


///
/// Function name	: Proce_Image1
///
/// Author			: Fabian Kung
///
/// Last modified	: 30 March 2019
///
/// Code Version	: 0.98
///
/// Processor		: ARM Cortex-74 family
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
void Proce_Image1(TASK_ATTRIBUTE *ptrTask)
{
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

	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization and check if camera is ready.
			if (gnCameraReady == _CAMERA_READY)
			{
				gnCameraLED = 1;					// Set camera LED intensity to low.
				nCurrentFrame = gnFrameCounter;
				OSSetTaskContext(ptrTask, 1, 10*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 10 msec.
			}
			else
			{
				OSSetTaskContext(ptrTask, 0, 1*__NUM_SYSTEMTICK_MSEC);     // Next state = 0, timer = 1 msec.
			}
			break;

			case 1: // State 1 - Wait until a new frame is acquired in the image buffer before start processing.
			if (gnFrameCounter != nCurrentFrame)		// Check if new image frame has been captured.
			{
				nCurrentFrame = gnFrameCounter;			// Update current frame counter.
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
				PIN_FLAG2_SET;								// Set indicator flag, this is optional, for debugging purpose.
				
				nCounter = 0;
				
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
				OSSetTaskContext(ptrTask, 2, 1);			// Next state = 2, timer = 1.
				
				nXMoment = 0;
				nXCount = 0;
				nYMoment = 0;
				nYCount = 0;
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);     // Next state = 1, timer = 1.
			}
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
				OSSetTaskContext(ptrTask, 3, 1);									// Next state = 3, timer = 1.
				break;																// Exit current state.
			}
			
			for (nXindex = 0; nXindex < gnImageWidth; nXindex++)					// Scan 2nd of image frame.
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
																					// in the external display.
				OSSetTaskContext(ptrTask, 3, 1);									// Next state = 3, timer = 1.
			}
			else																	// Not yet last row.
			{
				OSSetTaskContext(ptrTask, 2, 1);									// Next state = 2, timer = 1.
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

			OSSetTaskContext(ptrTask, 4, 1);										// Next state = 4, timer = 1.
			break;
			
			case 4: // State 4 - Transmit status to external controller, part 1 (transmit first 2 bytes).
					// NOTE: 28 Dec 2016.  To prevent overflow error on the remote controller (as it does not use DMA on the UART),
					// we avoid sending all 4 bytes one shot, but split the data packet into two packets or 2 bytes each.  A 1 msec
					// delay is inserted between each two bytes packet.  As the algorithm in the remote controller improves in future
					// this artificial restriction can be removed.

			if (gSCIstatus2.bTXRDY == 0)	// Check if any data to send via UART.
			{
				gbytTXbuffer2[0] = gnImageProcessingAlgorithm;	// Load data, process ID.
				gbytTXbuffer2[1] = gunMaxLuminance;				// Peak luminance value for this frame.
				gbytTXbuflen2 = 2;								// Set TX frame length.
				gSCIstatus2.bTXRDY = 1;							// Initiate TX.
				OSSetTaskContext(ptrTask, 5, 1*__NUM_SYSTEMTICK_MSEC);     // Next state = 5, timer = 1 msec.
			}
			else
			{
				OSSetTaskContext(ptrTask, 4, 1);				// Next state = 4, timer = 1.
			}
			break;

			case 5: // State 5 - Transmit status to external controller, part 2 (transmit last 2 bytes).
			if (gSCIstatus2.bTXRDY == 0)			// Check if any data to send via UART.
			{	
				gbytTXbuffer2[0] = nxmax[0];
				gbytTXbuffer2[1] = nymax[0];
				gbytTXbuflen2 = 2;					// Set TX frame length.
				gSCIstatus2.bTXRDY = 1;				// Initiate TX.
				OSSetTaskContext(ptrTask, 6, 1*__NUM_SYSTEMTICK_MSEC);    // Next state = 6, timer = 1 msec.
			}
			else
			{
				OSSetTaskContext(ptrTask, 5, 1);    // Next state = 5, timer = 1.
			}
			break;

			case 6:	// State 6 - Check if process ID has changed.
				PIN_FLAG2_CLEAR;					// Clear indicator flag, this is optional, for debugging purpose.
				OSSetTaskContext(ptrTask, 1, 1);	// Next state = 1, timer = 1.			
			break;

			default:
			OSSetTaskContext(ptrTask, 0, 1);		// Back to state = 0, timer = 1.
			break;
		}
	}
}

