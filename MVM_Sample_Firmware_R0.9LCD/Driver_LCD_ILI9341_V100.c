
//
// File				: Drivers_LCD_ILI9340_V100.c
// Author(s)		: Fabian Kung
// Last modified	: 31 Oct 2020
// Toolsuites		: Atmel Studio 7.0 or later
//					  GCC C-Compiler
//					  ARM CMSIS 5.0.1		

#include "osmain.h"
#include "spi0.h"	// NOTE: 8 April 2019
#include "spi.h"	// It seems CMSIS 5.0.1 and above provided by Atmel remove support for SPI0 and SPI1, 
					// they only provide support for QSPI.  Thus I had to explicitly include the header
					// files for SPI.
					
#include "./Driver_TCM8230_LCD.h"	// For QVGA (color pixel data) and QQVGA (grayscale) image.


// NOTE: Public function prototypes are declared in the corresponding *.h file.


//
// --- PUBLIC VARIABLES ---
//         
unsigned int	gunHue;					// For debug purpose.

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

unsigned int	gunIPResult[_IMAGE_HRESOLUTION/4][_IMAGE_VRESOLUTION];  // 2D integer array to store image processing algorithm result,
																		// limited to unsigned integer 8 bits for each pixel.


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

//
// --- PRIVATE VARIABLES ---
//

#define		_TFT_LCD_HRESOLUTION	320
#define		_TFT_LCD_VRESOLUTION	240

//#define PIN_DC_SET				PIOA->PIO_ODSR |= PIO_ODSR_P8;					
//#define PIN_DC_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P8;		
#define PIN_DC_SET				PIOD->PIO_ODSR |= PIO_ODSR_P12;
#define PIN_DC_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P12;	

#define ILI9341_TFTWIDTH   240      ///< ILI9341 max TFT width
#define ILI9341_TFTHEIGHT  320      ///< ILI9341 max TFT height

#define ILI9341_NOP        0x00     ///< No-op register
#define ILI9341_SWRESET    0x01     ///< Software reset register
#define ILI9341_RDDID      0x04     ///< Read display identification information
#define ILI9341_RDDST      0x09     ///< Read Display Status

#define ILI9341_SLPIN      0x10     ///< Enter Sleep Mode
#define ILI9341_SLPOUT     0x11     ///< Sleep Out
#define ILI9341_PTLON      0x12     ///< Partial Mode ON
#define ILI9341_NORON      0x13     ///< Normal Display Mode ON

#define ILI9341_RDMODE     0x0A     ///< Read Display Power Mode
#define ILI9341_RDMADCTL   0x0B     ///< Read Display MADCTL
#define ILI9341_RDPIXFMT   0x0C     ///< Read Display Pixel Format
#define ILI9341_RDIMGFMT   0x0D     ///< Read Display Image Format
#define ILI9341_RDSELFDIAG 0x0F     ///< Read Display Self-Diagnostic Result

#define ILI9341_INVOFF     0x20     ///< Display Inversion OFF
#define ILI9341_INVON      0x21     ///< Display Inversion ON
#define ILI9341_GAMMASET   0x26     ///< Gamma Set
#define ILI9341_DISPOFF    0x28     ///< Display OFF
#define ILI9341_DISPON     0x29     ///< Display ON

#define ILI9341_CASET      0x2A     ///< Column Address Set
#define ILI9341_PASET      0x2B     ///< Page Address Set
#define ILI9341_RAMWR      0x2C     ///< Memory Write
#define ILI9341_RAMRD      0x2E     ///< Memory Read

#define ILI9341_PTLAR      0x30     ///< Partial Area
#define ILI9341_MADCTL     0x36     ///< Memory Access Control
#define ILI9341_VSCRSADD   0x37     ///< Vertical Scrolling Start Address
#define ILI9341_PIXFMT     0x3A     ///< COLMOD: Pixel Format Set

#define ILI9341_FRMCTR1    0xB1     ///< Frame Rate Control (In Normal Mode/Full Colors)
#define ILI9341_FRMCTR2    0xB2     ///< Frame Rate Control (In Idle Mode/8 colors)
#define ILI9341_FRMCTR3    0xB3     ///< Frame Rate control (In Partial Mode/Full Colors)
#define ILI9341_INVCTR     0xB4     ///< Display Inversion Control
#define ILI9341_DFUNCTR    0xB6     ///< Display Function Control

#define ILI9341_PWCTR1     0xC0     ///< Power Control 1
#define ILI9341_PWCTR2     0xC1     ///< Power Control 2
#define ILI9341_PWCTR3     0xC2     ///< Power Control 3
#define ILI9341_PWCTR4     0xC3     ///< Power Control 4
#define ILI9341_PWCTR5     0xC4     ///< Power Control 5
#define ILI9341_VMCTR1     0xC5     ///< VCOM Control 1
#define ILI9341_VMCTR2     0xC7     ///< VCOM Control 2

#define ILI9341_RDID1      0xDA     ///< Read ID 1
#define ILI9341_RDID2      0xDB     ///< Read ID 2
#define ILI9341_RDID3      0xDC     ///< Read ID 3
#define ILI9341_RDID4      0xDD     ///< Read ID 4

#define ILI9341_GMCTRP1    0xE0     ///< Positive Gamma Correction
#define ILI9341_GMCTRN1    0xE1     ///< Negative Gamma Correction
//#define ILI9341_PWCTR6     0xFC

// Color definitions
#define ILI9341_BLACK       0x0000  ///<   0,   0,   0
#define ILI9341_NAVY        0x000F  ///<   0,   0, 123
#define ILI9341_DARKGREEN   0x03E0  ///<   0, 125,   0
#define ILI9341_DARKCYAN    0x03EF  ///<   0, 125, 123
#define ILI9341_MAROON      0x7800  ///< 123,   0,   0
#define ILI9341_PURPLE      0x780F  ///< 123,   0, 123
#define ILI9341_OLIVE       0x7BE0  ///< 123, 125,   0
#define ILI9341_LIGHTGREY   0xC618  ///< 198, 195, 198
#define ILI9341_DARKGREY    0x7BEF  ///< 123, 125, 123
#define ILI9341_BLUE        0x001F  ///<   0,   0, 255
#define ILI9341_GREEN       0x07E0  ///<   0, 255,   0
#define ILI9341_CYAN        0x07FF  ///<   0, 255, 255
#define ILI9341_RED         0xF800  ///< 255,   0,   0
#define ILI9341_MAGENTA     0xF81F  ///< 255,   0, 255
#define ILI9341_YELLOW      0xFFE0  ///< 255, 255,   0
#define ILI9341_WHITE       0xFFFF  ///< 255, 255, 255
#define ILI9341_ORANGE      0xFD20  ///< 255, 165,   0
#define ILI9341_GREENYELLOW 0xAFE5  ///< 173, 255,  41
#define ILI9341_PINK        0xFC18  ///< 255, 130, 198

// Initialization commands for ILI9341
// Format:
// [Command] [No. of argument bytes = N] [Argument0] [Argument1] ... [Argument N-1] 
const unsigned char bytInitCmd[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9341_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9341_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9341_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9341_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9341_MADCTL  , 1, 0x48,             // Memory Access Control
  ILI9341_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9341_PIXFMT  , 1, 0x55,
  ILI9341_FRMCTR1 , 2, 0x00, 0x18,
  ILI9341_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9341_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9341_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
  0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  ILI9341_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
  0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  ILI9341_SLPOUT  , 0x00,                // Exit Sleep
  ILI9341_DISPON  , 0x00,                // Display on
  0x00                                   // End of list
};
		

#define		_TFT_LCD_XOFFSET	0
#define		_TFT_LCD_YOFFSET	0

//
// --- Process Level Constants Definition --- 
//

//
// --- PRIVATE FUNCTIONS ---
//
int nSPI0WriteCommand(unsigned char );
int	nSPI0WriteData8(unsigned char );
int	nSPI0WriteData16(unsigned int );

///
/// Process name	: Proce_LCD_ILI9341_Driver
///
/// Author			: Fabian Kung
///
/// Last modified	: 30 Oct 2020
///
/// Code version	: 0.65
///
/// Processor		: ARM Cortex-M7 family                   
///
/// Processor/System Resource 
/// PINS		: 1. Pin PD21 = MOSI, Peripheral B.
///               2. Pin PD22 = SCLK, Peripheral B.
///               3. Pin PB2 = CS1, Peripheral D.
///               4. Pin PA8 = D/C.
///                  or Pin PD12
///
/// MODULES		: 1. SPI0 (Internal).
///
/// RTOS		: Ver 1 or above, round-robin scheduling.
///
/// Global variable	: gunImgAtt[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION]
///

#ifdef 				  __OS_VER		// Check RTOS version compatibility.
	#if 			  __OS_VER < 1
		#error "Proce_SerialTFTDisplay_Driver: Incompatible OS version"
	#endif
#else
	#error "Proce_SerialTFTDisplay_Driver: An RTOS is required with this function"
#endif

///
/// Description		: 
/// This process drives 320x240 color TFT LCD display controlled by ILI9340/ILI9341 LCD controller.
/// It needs to work in conjunction with a camera driver, and it access the pixel data
/// contains in the image attribute buffer (IAB) gunImgAtt[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION] 
/// of the camera driver. The interface mode is assumed to be
/// 4-wire SPI (CS, SCK, MOSI and Data/Command) Mode 1. In particular this driver is based on 
/// Limor "ladyada" Fried Arduino C++ library written for Adafruit Industries 2.2", 2.4", 2.8" 
/// and 3.2" series color TFT display. 
///
/// References:
/// 1. Adafruit Arduino Library in C++ for ILI9341 in Github.
/// 2. ILI9340 preliminary datasheet (V0.12).

#define PIN_FLAG1_SET			PIOA->PIO_ODSR |= PIO_ODSR_P7;		// Set flag.  Flag 1 is used to mark the start and end of a frame.
#define PIN_FLAG1_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P7;		// Clear flag.
#define PIN_FLAG2_SET			PIOA->PIO_ODSR |= PIO_ODSR_P8;		// Set flag.  Flag 2 is used to mark the start and end of a line.
#define PIN_FLAG2_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P8;		// Clear flag.

void Proce_LCD_ILI9341_Driver(TASK_ATTRIBUTE *ptrTask)
{
	int				nXTemp;
	int				nYTemp;
	int				nTemp;
	unsigned int	unResult;
	unsigned int	unTemp;
	unsigned int	unL5;
	unsigned int	unL6;
	static int		nArgCount;
	unsigned char	bytCmd;
	static	int		nIndex;
	static	int		nXindex = 0;
	static  int		nYindex = 0;
	
	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Setup SPI0.  Default state is:
				// Master.
				// 8 or 16 bits data.
				// NSS (chip select) tied to device.
				// SPI clock = 25-75 MHz.
				// Master clock polarity: 0 when idle, data is captured on low-to-high transition.
				// 
				// 24 Nov 2015: To enable a peripheral, we need to:
				// 1. Assign the IO pins to the peripheral.
				// 2. Select the correct peripheral block (A, B, C or D).
				PIOD->PIO_PDR = (PIOD->PIO_PDR) | PIO_PDR_P21;	// Set PD21 and PD22 to be controlled by Peripheral.
				PIOD->PIO_PDR = (PIOD->PIO_PDR) | PIO_PDR_P22;	
				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P2;	// Set PB2 controlled by Peripheral.
																// SPI0 (Three-wire Interface 1) resides in
																// in Peripheral block B and D, with
																// PD21 = MOSI
																// PD22 = SPCK
																// PB2 = NSS
				PIOD->PIO_ABCDSR[0] = (PIOD->PIO_ABCDSR[0]) | PIO_ABCDSR_P21;	// Select peripheral block B for
				PIOD->PIO_ABCDSR[1] = (PIOD->PIO_ABCDSR[1]) & ~PIO_ABCDSR_P21;	// PD21.
				PIOD->PIO_ABCDSR[0] = (PIOD->PIO_ABCDSR[0]) | PIO_ABCDSR_P22;	// Select peripheral block B for
				PIOD->PIO_ABCDSR[1] = (PIOD->PIO_ABCDSR[1]) & ~PIO_ABCDSR_P22;	// PD22.
				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) | PIO_ABCDSR_P2;	// Select peripheral block D for
				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) | PIO_ABCDSR_P2;	// PB2.							
				PMC->PMC_PCER0 |= PMC_PCER0_PID21;		// Enable peripheral clock to SPI0 module (ID21)
				REG_SPI0_MR |= SPI_MR_MSTR;				// Set SPI0 to master mode.	
				REG_SPI0_CSR0 |= SPI_CSR_NCPHA;			// Set master clock polarity, data is captured on low-to-high transition. 
				//REG_SPI0_CSR0 |= SPI_CSR_SCBR(6);		// Set SCK clock rate = fperipheral/SCBR = 25 MHz.  fperipheral = 150 MHz.
				//REG_SPI0_CSR0 |= SPI_CSR_SCBR(3);		// Set SCK clock rate = fperipheral/SCBR = 50 MHz.  fperipheral = 150 MHz.
				REG_SPI0_CSR0 |= SPI_CSR_SCBR(2);		// Set SCK clock rate = fperipheral/SCBR = 75 MHz.  fperipheral = 150 MHz.
				REG_SPI0_CSR0 &= ~SPI_CSR_BITS_Msk;		// Clear all bits in CSR related to BITS.
				REG_SPI0_CSR0 |= SPI_CSR_BITS(0) ;		// Set data length to 8 bits.
				//REG_SPI0_CSR0 |= SPI_CSR_BITS(8) ;	// Set data length to 16 bits.
				PIN_DC_SET;								// Set D/C pin.
				REG_SPI0_CR |= SPI_CR_SPIEN;			// Enable SPI0, here TDRE flag in REG_SPI0_SR will be set.				
				OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);		// Next state = 1, timer = 1000 msec.
			break;
			
			case 1: // State 1 - Set software reset command to LCD display.			
				if (gnCameraReady == _CAMERA_READY)								// Check if camera is ready.
				{
					if (nSPI0WriteCommand(ILI9341_SWRESET) > 0)					// Software reset, if SPI0 write is successful.
					{
						nIndex = 0;
						OSSetTaskContext(ptrTask, 2, 200*__NUM_SYSTEMTICK_MSEC);	// Next state = 2, timer = 200 msec.
					}
					else
					{
						OSSetTaskContext(ptrTask, 1, 100*__NUM_SYSTEMTICK_MSEC);	// Next state = 1, timer = 100 msec.		
					}
				}
				else
				{
					OSSetTaskContext(ptrTask, 1, 100*__NUM_SYSTEMTICK_MSEC);		// Next state = 1, timer = 100 msec.	
				}
			break;
		
			case 2: // State 2 - Initialize LCD display - Part 1: Send command.
				bytCmd = bytInitCmd[nIndex++];				// Get command and increment index.
				if (bytCmd > 0)								// Is it end of list?	
				{
					nSPI0WriteCommand(bytCmd);				// Send command.
					nArgCount = bytInitCmd[nIndex++];		// Get no. of argument and increment index.
					if (nArgCount > 0)						// If there is argument to send.
					{
						OSSetTaskContext(ptrTask, 3, 1);	// Next state = 3, timer = 1.
					}
					else                                    // Commands with no arguments such as display on or Exit Sleep 
															// need to include a long delay.
					{
						OSSetTaskContext(ptrTask, 2, 150*__NUM_SYSTEMTICK_MSEC);	// Next state = 2, timer = 150 msec.
					}
				}
				else                                        // If command = 0, it signify end of command list.   
				{
					nXindex = 0;
					nYindex = 0;
					//gnCameraLED = 2;						// Turn on camera module LED.
					OSSetTaskContext(ptrTask, 4, 100*__NUM_SYSTEMTICK_MSEC);		// Next state = 4, timer = 100 msec.			
				}							
			break;
			
			case 3: // State 3 - Initialize LCD display - Part 2: Send command's argument. 								
				nSPI0WriteData8(bytInitCmd[nIndex++]);		// Send command argument and increment index.
				nArgCount--;
				if (nArgCount > 0)							// If still got argument.
				{
					OSSetTaskContext(ptrTask, 3, 1);		// Next state = 3, timer = 1.
				}
				else
				{
					OSSetTaskContext(ptrTask, 2, 1);		// Next state = 2, timer = 1.
				}
			break;
			
			case 4: // State 4 - Update pixel (x1 resolution) - For QVGA resolution. 
				// 13 Oct 2020: The following setting is optimize for SysTick = 166.67 usec.  Experiments indicate that
				// this task, camera driver task, and camera LED task falls within the duration of SysTick.  One cycle
				// of this state takes around 70usec to complete.
				
				// Here we can update four pixels in one shot if the SCK frequency is 25 MHz.
				// If we increase the SCK frequency to 50 MHz, we can update up to 8 pixels in one shot, increasing the
				// refresh rate.
				// Increasing the SCK frequency to 75 MHz, we can update up to 12 pixels in one shot.
				// If the 7-bits luminance value is provided, we can convert in to RGB565 format as 
				// follows:
				//unTemp = (gunImgAtt[nXindex][nYindex] & _LUMINANCE_MASK);	// Get 7-bits luminance.
				//unR = (unTemp<<9) & 0xF800;					// Red component.
				//unG = (unTemp<<4) & 0x07E0;					// Green component.
				//unB = unTemp>>2;								// Blue component.
				//nSPI0WriteData16(unB + unG + unR);			
				
				// Note:
				// From the datasheet ILI9431, the column address set and row/page address set requires 2 16-bits parameters 
				// or 4 8-bits parameters.  These are the start and end address of the respective column or row.  Thus we
				// need to run the nSPI0WriteData16() function twice even if the start and end address are the same.  I have
				// verified that if not, the write to video RAM command cannot be issued repeatedly.  The column address set
				// is optional. If not set, the previous start and end value will be used.  
				
				//PIN_FLAG2_SET;
				//nSPI0WriteCommand(ILI9341_CASET);				// Column address set.  
				//nSPI0WriteData16(nYindex);
				//nSPI0WriteData16(nYindex);				
				nSPI0WriteCommand(ILI9341_PASET);				// Row/page address set.
				nSPI0WriteData16(nXindex);						// Start row address.				
				nSPI0WriteData16(nXindex+159);					// End row address.
				nSPI0WriteCommand(ILI9341_RAMWR);				// Write to video RAM.
				
				REG_SPI0_CSR0 &= ~SPI_CSR_BITS_Msk;				// Clear all bits in CSR related to BITS.
				REG_SPI0_CSR0 |= SPI_CSR_BITS(8) ;				// Set data length to 16 bits.
				for (nIndex = 0; nIndex < 160; nIndex++)		// 160 pixels
				{	
					#ifdef		__QQVGA__
					// Display luminance (grayscale) pixel data on QVGA color TFT LCD 
																				// Load 16-bits pixel data to SPI Transmit Data Register (TDR).
																				// For QQVGA resolution the RGB or luminance of one image pixel
																				// is expanded to 4 pixels, thus the division operation in the
																				// x and y indices of the gnImgAtt.
					nXTemp = (nXindex + nIndex)>>1;
					nYTemp = nYindex>>1;					
																																								
					nTemp = nXTemp / 4;				// Shrink the column index by 4, because each unsigned integer contains 4 bytes.
					switch (nXTemp % 4)				// The unsigned integer format is 32-bits in ARM Cortex-M.  However each pixel
					// computation result is 8-bits, thus each unsigned integer word can store results
					// for 4 pixels.
					// Bit 0-7:		pixel 1
					// Bit 8-15:	pixel 2
					// Bit 16-23:	pixel 3
					// Bit 24-31:	pixel 4
					{
						case 0:	// nIndex % 4 = 0	(nIndex = 0,4,8 etc)
							unResult = gunIPResult[nTemp][nYTemp] & 0x000000FF;	// Get the lowest 8-bits from 32-bits word.
						break;
							
						case 1: // nIndex % 4 = 1	(nIndex = 1,5,9 etc)
							unResult = (gunIPResult[nTemp][nYTemp] & 0x0000FF00)>>8;	// Get the 2nd 8-bits from 32-bits word.
						break;
							
						case 2: // nIndex % 4 = 2	(nIndex = 2,6,10 etc)
							unResult = (gunIPResult[nTemp][nYTemp] & 0x00FF0000)>>16;	// Get the 3rd 8-bits from 32-bits word.
						break;
							
						default: // nIndex % 4 = 3	(nIndex = 3,7,11 etc)
							unResult = gunIPResult[nTemp][nYTemp]>>24;			// Get the highest 8-bits from 32-bits word.
						break;
					}																				
																				
					if (unResult == 255)											// If the pixel in question fits the IPA criteria of a lump of pixels
					{																// matching the color selected.
						REG_SPI0_TDR = 0xFFE0;										// Set pixel to yellow in LCD display.					
					}	
					else
					{
						unTemp = (gunImgAtt[nXTemp][nYTemp] & _LUMINANCE_MASK);		// Get 7-bits luminance.		
						unL6 = unTemp>>1;											// Convert the 7-bits luminance value to RGB565 format.
						unL5 = unL6>>1;												// Let L = 7-bits luminance value.
						REG_SPI0_TDR = (unL5*2049) + (unL6*32);						// L5 = 5-bits luminance value = L>>2.
						// L6 = 6-bits luminance value = L>>1.
						//unL5 = unL6>>2;											// There are two methods to get a greyscale RGB565 pixel value
						//REG_SPI0_TDR = unL5*2113;									// from the luminance.  The 1st approach is slightly faster but
																					// less resolution.
																					// RGB565 = L5 + L5*(2^11) + L5*(2^6)
																					//        = L5*(1 + 2048 + 64) = L5*2113
																					// Or
																					// RGB565 = L5 + L5*(2^11) + L6*(2^5)
																					//        = L5*(2049) + L6*32						
					}																							
					#else	
					// Display color (RGB565) pixel data on QVGA color TFT LCD 
					REG_SPI0_TDR = gunImgAtt[nXindex + nIndex][nYindex];			// Load 16-bits pixel data in RGB565 format
																					// to SPI Transmit Data Register (TDR).
					#endif
					while ((REG_SPI0_SR & SPI_SR_TXEMPTY) == 0)	{} // Wait until TXEMPTY flag is set.
					
					// NOTE: 13 Oct 2020, test code.  Somehow this does not work!
					//while ((REG_SPI0_SR & SPI_SR_TDRE) == 0)	{} // Wait until TDRE flag is set. 
					//REG_SPI0_TDR = gnImgAtt[nXindex + nIndex][nYindex];
				}
				nXindex = nXindex + 160;						// Update x index.
							
				if ((nXindex) == _TFT_LCD_HRESOLUTION)			// Update row and column indices
				{												// and check for end of row.
					nXindex = 0;
					if ((nYindex++) == _TFT_LCD_VRESOLUTION)
					{
						nYindex = 0;
					}
					nSPI0WriteCommand(ILI9341_CASET);			// Generate new column address set.
					nSPI0WriteData16(nYindex);					// Here the start and end column are the same.
					nSPI0WriteData16(nYindex);			
				}							
				OSSetTaskContext(ptrTask, 4, 1);			// Next state = 4, timer = 1.
				//PIN_FLAG2_CLEAR;
			break;
						
			case 10: // State 10 - Idle.
				nIndex = 0;
				//OSSetTaskContext(ptrTask, 10, 100*__NUM_SYSTEMTICK_MSEC);		// Next state = 10, timer = 100 msec.
				OSSetTaskContext(ptrTask, 2, 5*__NUM_SYSTEMTICK_MSEC);		// Next state = 2, timer = 5 msec.
			break;
			
			default:
				OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}

__INLINE int nSPI0WriteCommand(unsigned char bytCommand)
{
	if ((REG_SPI0_SR & SPI_SR_TDRE)>0)			// Make sure Transmit Data Register Empty flag is set, means no data
												// in the waiting register.
	{
		PIN_DC_CLEAR;							// Clear D/C pin, indicate to LCD controller data is command.
		REG_SPI0_CSR0 &= ~SPI_CSR_BITS_Msk;		// Clear all bits in CSR related to BITS.
		REG_SPI0_CSR0 |= SPI_CSR_BITS(0) ;		// Set data length to 8 bits.	
		REG_SPI0_TDR = bytCommand;				// Send 8-bits command.	
		while ((REG_SPI0_SR & SPI_SR_TXEMPTY) == 0)	{} // Wait until TXEMPTY flag is set.
		PIN_DC_SET;								// Set D/C pin.
		return 1;
	}
	else
	{
		return 0;
	}
}

__INLINE int nSPI0WriteData8(unsigned char bytData)
{
	if ((REG_SPI0_SR & SPI_SR_TDRE)>0)			// Make sure Transmit Data Register Empty flag is set, means no data
												// in the waiting register.
	{
		REG_SPI0_CSR0 &= ~SPI_CSR_BITS_Msk;		// Clear all bits in CSR related to BITS.
		REG_SPI0_CSR0 |= SPI_CSR_BITS(0) ;		// Set data length to 8 bits.
		REG_SPI0_TDR = bytData;					// Send 8-bits data.
		while ((REG_SPI0_SR & SPI_SR_TXEMPTY) == 0)	{} // Wait until TXEMPTY flag is set.
		return 1;
	}
	else
	{
		return 0;
	}
}

__INLINE int nSPI0WriteData16(unsigned int unData)
{
	if ((REG_SPI0_SR & SPI_SR_TDRE)>0)			// Make sure Transmit Data Register Empty flag is set, means no data
												// in the waiting register.
	{
		REG_SPI0_CSR0 &= ~SPI_CSR_BITS_Msk;		// Clear all bits in CSR related to BITS.
		REG_SPI0_CSR0 |= SPI_CSR_BITS(8) ;		// Set data length to 16 bits.
		REG_SPI0_TDR = unData;					// Send 16-bits data.
		while ((REG_SPI0_SR & SPI_SR_TXEMPTY) == 0)	{} // Wait until TXEMPTY flag is set.
		return 1;
	}
	else
	{
		return 0;
	}
}