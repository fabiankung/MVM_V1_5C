//////////////////////////////////////////////////////////////////////////////////////////////
//
//	USER DRIVER ROUTINES DECLARATION (PROCESSOR DEPENDENT) 
//   
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: User_Task.c
// Author(s)		: Fabian Kung
// Last modified	: 25 Nov 2021
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.4.0

#include "osmain.h"
#include "Driver_UART2_V100.h"
#include "Driver_USART0_V100.h"
#include "Driver_ESP8266_V100.h"
#include "Driver_I2C1_V100.h"
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
uint8_t  gbytESP8266_TXBuffer[20];       // Transmit buffer for ESP8266 AT commands.

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
/// Last modified	: 25 Nov 2021
///
/// Code Version	: 1.00
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

#define _PROCE_STREAMIMAGE_TIMEOUT_PERIOD	50*__NUM_SYSTEMTICK_MSEC // Set timeout period, 50 msec.

void Proce_MessageLoop_StreamImage(TASK_ATTRIBUTE *ptrTask)
{
	static unsigned char bytData;
	static int nLineCounter;
	static int nXposCounter;
	int nCurrentPixelData;
	int nRefPixelData;
	int nRepetition;
	int nTemp;
	int nIndex;
	static	int nTimeoutCounter = 0;
	static  int nDigit, nTen, nHundred;
		
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization.
			objMarker1.nAttrib = 0;			// Disable plotting of all markers in the graphic window of Remote host.
			objMarker2.nAttrib = 0;
			objMarker3.nAttrib = 0;
			PIN_LED2_CLEAR;					// Turn off indicator LED2.
			nLineCounter = 0;
			OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 1000 msec.
			break;
			
			case 1: // State 1 - Wait until ESP8266 module is ready.  This is indicated by the flag gnESP8266Flag = 0.
			if (gnESP8266Flag >= 0)
			{
				OSSetTaskContext(ptrTask, 2, 100*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 100 msec
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 100*__NUM_SYSTEMTICK_MSEC);     // Next state = 1, timer = 100 msec
			}
			break;

			case 2: // State 2 - Check gnESP8266Flag and dispense task.
			if (gnESP8266Flag == 1)
			{
				OSSetTaskContext(ptrTask, 3, 1);					// Next state = 3, timer = 1.
			}
			else if (gnESP8266Flag == 2)
			{
				OSSetTaskContext(ptrTask, 4, 1);					// Next state = 4, timer = 1.
			}
			else if (gnESP8266Flag == 0)
			{
				OSSetTaskContext(ptrTask, 2, 1);					// Next state = 2, timer = 1.
			}			
			else
			{
				OSSetTaskContext(ptrTask, 0, 100*__NUM_SYSTEMTICK_MSEC);	// Next state = 0, timer = 100 msec.
			}
			break;
			
			case 3: // State 3 - Prepare data to send and request ESP8266 for transmission.
			// a) wait for start signal (the character 'X') from remote host on UART2.
			// Once start signal received, pre-process 1 row of pixel data.
			//PIOA->PIO_ODSR |= PIO_ODSR_P8;					// Set flag 4.
			// --- Message clearing and stream video image for WiFi connection ---
			if ((gnESP8266Flag == 1) && (gSCIstatus.bTXRDY == 0))	// The flag gnESP8266Flag is 1 if remote client send the character 'X'.
			{														// Also make sure UART2 is idle.
				
				// Perform run-length encoding compression on 1 line of pixel data.
				gbytTXbuffer[0] = 0xFF;					// Start of line code.
				gbytTXbuffer[1] = nLineCounter;			// Line number.
				// gbytTXbuffer[2] stores the payload length.
				// --- Implement simple RLE algorithm ---
				nRepetition = 0;						// Initialize repetition counter.
				nXposCounter = 1;						// Initialize x-position along a line of pixels.
				// Initialize the first data byte.
				// Get 1st byte/pixel.
				nCurrentPixelData = gunImgAtt[0][nLineCounter] & _LUMINANCE_MASK; // Get pixel luminance data from frame buffer 1, 1st byte.
				nCurrentPixelData = nCurrentPixelData>>1;	// Mask out bit7 of luminance data. Note: Only 7-bits data is allowed
				nRefPixelData = nCurrentPixelData;		// Setup the reference value.
				gbytTXbuffer[3] = nCurrentPixelData;	// Prepare first byte of the data payload.
				// Get subsequent bytes/pixels in the line of pixels data.
				for (nIndex = 1; nIndex < gnImageWidth; nIndex++)
				{
					// Send luminance data.
					nCurrentPixelData = gunImgAtt[nIndex][nLineCounter] & _LUMINANCE_MASK; // Get pixel luminance data from frame buffer 1. Subsequent bytes.
					
					if (nCurrentPixelData == nRefPixelData) // Current and previous pixels share similar value.
					{
						nRepetition++;
						if (nIndex == gnImageWidth-1)	   // Check for last pixel in line.
						{
							gbytTXbuffer[nXposCounter+3] = 128 + nRepetition; // Set bit7 and add the repetition count.
							nXposCounter++;					// Point to next byte in the array.
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
				
				// Generate number of bytes to transmit via WiFi in BCD.
				nHundred = 0;
				nTen = 0;
				nDigit = nXposCounter + 3;					// nXposCounter now is the payload length. We need to add in the count for start-of-line 
															// code, line number and payload length.
				if (nDigit > 99)
				{
					nHundred = 1;
					nDigit = nDigit - 100;
					while (nDigit > 9)						// Find the digit for tenth.
					{
						nDigit = nDigit - 10;
						nTen++;
					}										// After coming out of the while() loop, nDigit will be < 10.
				}
				else										// nXposCounter < 100.
				{
					while (nDigit > 9)						// Find the digit for tenth.
					{
						nDigit = nDigit - 10;
						nTen++;
					}										// After coming out of the while() loop, nDigit will be < 10.
				}
				
				gbytESP8266_TXBuffer[0] = 'A';					// Ask ESP8266 to send specific data bytes to channel 0.
				gbytESP8266_TXBuffer[1] = 'T';					//
				gbytESP8266_TXBuffer[2] = '+';					//
				gbytESP8266_TXBuffer[3] = 'C';					//
				gbytESP8266_TXBuffer[4] = 'I';					//
				gbytESP8266_TXBuffer[5] = 'P';					//
				gbytESP8266_TXBuffer[6] = 'S';					//
				gbytESP8266_TXBuffer[7] = 'E';					//
				gbytESP8266_TXBuffer[8] = 'N';					//
				gbytESP8266_TXBuffer[9] = 'D';					//
				gbytESP8266_TXBuffer[10] = '=';					//
				gbytESP8266_TXBuffer[11] = '0';					//
				gbytESP8266_TXBuffer[12] = ',';					// Up to here total bytes is 13.
				
				if (nHundred == 0)
				{
					if (nTen == 0)								// Less than 10 bytes to transmit.
					{
						gbytESP8266_TXBuffer[13] = nDigit + 48;	// Convert integer 0-9 into ASCII.
						gbytESP8266_TXBuffer[14] = 0xD;			// Carriage Return or CR.  Note: always end commend with NL and CR.
						gbytESP8266_TXBuffer[15] = 0xA;			// NL or '\n'
						nTemp = 16;								// Total no. of bytes to send via WiFi.
					}
					else                                        // Between 10-99 bytes to transmit.
					{
						gbytESP8266_TXBuffer[13] = nTen + 48;	// Convert integer 0-9 into ASCII.
						gbytESP8266_TXBuffer[14] = nDigit + 48;	// Convert integer 0-0 into ASCII.
						gbytESP8266_TXBuffer[15] = 0xD;			// Carriage Return or CR.  Note: always end commend with NL and CR.
						gbytESP8266_TXBuffer[16] = 0xA;			// NL or '\n'
						nTemp = 17;								// Total no. of bytes to send via WiFi.
					}
				}
				else                                            // More than 100 bytes to transmit.
				{
					gbytESP8266_TXBuffer[13] = nHundred + 48;	// Convert integer 0-9 into ASCII.
					gbytESP8266_TXBuffer[14] = nTen + 48;		// Convert integer 0-9 into ASCII.
					gbytESP8266_TXBuffer[15] = nDigit + 48;		// Convert integer 0-0 into ASCII.
					gbytESP8266_TXBuffer[16] = 0xD;				// Carriage Return or CR.  Note: always end commend with NL and CR.
					gbytESP8266_TXBuffer[17] = 0xA;				// NL or '\n'
					nTemp = 18;									// Total no. of bytes to send via WiFi.
				}
				
				SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
				// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
				// from the cache, it may not contains the correct and up-to-date data.
				XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytESP8266_TXBuffer;	// Set source start address.
				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nTemp);	// Set number of bytes to transmit.
				XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
				gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
				gSCIstatus.bTXRDY = 1;										// Initiate TX.
				PIN_LED2_SET;												// Lights up indicator LED2.
				
				gnESP8266Flag = 0;								// Clear flag.
				nTimeoutCounter = 0;							// Reset timeout timer.
				OSSetTaskContext(ptrTask, 2, 1);				// Next state = 2, timer = 1.	
			}
			else
			{
				nTimeoutCounter++;											// Check for timeout.
				if (nTimeoutCounter > _PROCE_STREAMIMAGE_TIMEOUT_PERIOD)
				{
					gnESP8266Flag = 0;										// Clear flag.
				}
				OSSetTaskContext(ptrTask, 2, 1);						// Next state = 2, timer = 1.
			}
			//PIOA->PIO_ODSR &= ~PIO_ODSR_P8;							// Clear flag 4.
			break;
			
			case 4: // State 4 - Transmit a line of pixel data to the remote monitor via ESP8266.
			//PIOD->PIO_ODSR |= PIO_ODSR_P22;					// Set PD22.
			if ((gnESP8266Flag == 2)	&& (gSCIstatus.bTXRDY == 0))		// The flag is 2 if remote client send the character '>'.
			{
				SCB_CleanDCache();		// If we are using data cache (D-Cache), we should clean the data cache
				// (D-Cache) before enabling the DMA. Otherwise when XDMAC access data
				// from the cache, it may not contains the correct and up-to-date data.
				XDMAC->XDMAC_CHID[1].XDMAC_CSA = (uint32_t) gbytTXbuffer;	// Set source start address.
				XDMAC->XDMAC_CHID[1].XDMAC_CUBC = XDMAC_CUBC_UBLEN(nXposCounter+3);	// Set number of bytes to transmit. Remember
																			// to add in the 3 bytes of header.
				XDMAC->XDMAC_GE = XDMAC_GE_EN1;								// Enable channel 1 of XDMAC.
				gSCIstatus.bTXDMAEN = 1;									// Indicate UART transmit with DMA.
				gSCIstatus.bTXRDY = 1;										// Initiate TX.
				PIN_LED2_SET;												// Lights up indicator LED2.

				nLineCounter++;												// Point to next line of image pixel data.
				if (nLineCounter > gnImageHeight)							// Check if reach end of line.
				{
					nLineCounter = 0;										// Reset line counter.
				}
				
				gnESP8266Flag = 0;											// Clear flag.
				OSSetTaskContext(ptrTask, 2, 1);							// Next state = 2, timer = 1.
			}
			else
			{
				nTimeoutCounter++;											// Check for timeout.
				if (nTimeoutCounter > _PROCE_STREAMIMAGE_TIMEOUT_PERIOD)
				{
					gnESP8266Flag = 0;										// Clear flag.
					OSSetTaskContext(ptrTask, 2, 1);						// Next state = 2, timer = 1.
				}
				else
				{
					OSSetTaskContext(ptrTask, 4, 1);						// Next state = 4, timer = 1.
				}
			}
			//PIOD->PIO_ODSR &= ~PIO_ODSR_P22;								// Clear PD22.
			break;
						
			default:
			OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}