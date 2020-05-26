
//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_TCM8230.c
// Author(s)		: Fabian Kung
// Last modified	: 25 May 2020
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//                    ARM CMSIS 5.4.0
//                    SAMS70_DFP 2.3.88
//////////////////////////////////////////////////////////////////////////////////////////////
#include "osmain.h"
#include "Driver_I2C1_V100.h"

// NOTE: Public function prototypes are declared in the corresponding *.h file.

	
//
//
// --- PUBLIC VARIABLES ---
//
#define _IMAGE_HRESOLUTION   160		// For 160x120 pixels QQVGA resolution.
#define _IMAGE_VRESOLUTION   120
#define _NOPIXELSINFRAME	 19200

//#define _IMAGE_HRESOLUTION   128		// For 128x96 pixels subQCIF resolution.
//#define _IMAGE_VRESOLUTION   96
//#define _NOPIXELSINFRAME	 12288
				
int		gnFrameCounter = 0;
int		gnImageWidth = _IMAGE_HRESOLUTION;
int		gnImageHeight = _IMAGE_VRESOLUTION;
int		gnCameraLED = 0;
int		gnLuminanceMode = 0;		// Option to set how luminance value for each pixel
									// is computed from the RGB component.
									// 0 - Luminance (I) is computed from RGB using 
									//	   I = 0.250R + 0.625G + 0.125B = (2R + 5G + B)/8
									// 1 - I = 4R
									// 2 - I = 2G
									// Else - I = 4B

unsigned int gunImgAtt[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION];   // 1st image frame image attribute buffer.
unsigned int gunImgAtt2[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION];   // 2nd image frame image attribute buffer.
int		gnValidFrameBuffer;			// If equals 1, it means gunImgAtt[] data can be used for image processing.
									// If equals 2, then gunImgAtt2[] data can be used instead for image processing.
									// Under normal operation, the value of gnValidFrameBuffer will alternate
									// between 1 and 2 as raw pixel data captured from the camera is store in 
									// gunImgAtt[] and gunImgAtt2[] alternately.

// bit6 - bit0 = Luminance information, 7-bits.
// bit7 - Marker flag, set to 1 if we wish the external/remote display to highlight
//        this pixel. (Currently not used).
// bit16 - bit8 = Hue information, 9-bits, 0 to 360, No hue (gray scale) = 511.
// bit22 - bit17 = Saturation, 6-bits, 0-63 (63 = pure spectral).
// bit30 - bit23 = Luminance gradient, 8-bits.
// bit31 - Special flag. Can be used to store boolean type results of image processing algorithms.

#define     _LUMINANCE_MASK     0x0000007F  // luminance mask, bit6-0.
#define     _CLUMINANCE_MASK    0xFFFFFF80  // One's complement of luminance mask.
#define     _LUMINANCE_SHIFT    0
#define     _HUE_MASK           0x0001FF00  // bit16-8
#define     _HUE_SHIFT          8
#define     _SAT_MASK           0x003E0000  // Saturation mask, bit22-17.
#define     _CSAT_MASK          0xFFC1FFFF  // One's complement of saturation mask.
#define     _SAT_SHIFT          17
#define     _NO_HUE_BRIGHT      366         // Value for no hue when object is too bright or near grayscale.
#define     _NO_HUE_DARK		363         // Value for no hue when object is too dark.
// Valid hue ranges from 0 to 360.
#define     _GRAD_MASK          0x7F800000  // bit30-23
#define     _CGRAD_MASK         0x807FFFFF  // One's complement of gradient mask.
#define     _GRAD_SHIFT         23
#define     _MAX_GRADIENT       255

int16_t gunIHisto[255];    // Histogram for intensity, 255 levels.
unsigned int gunAverageLuminance = 0;

#define			_CAMERA_READY			1	// 1 = Camera module ready.
#define			_CAMERA_NOT_READY		0	// 0 = Camera module not ready.
int				gnCameraReady = _CAMERA_NOT_READY;		

// --- PRIVATE VARIABLES ---										
uint16_t gun16Pixel[_IMAGE_HRESOLUTION];		// Temporary buffer to store 1 line of pixel data (

// --- PRIVATE FUNCTION PROTOTYPES ---
inline int Min(int a, int b) {return (a < b)? a: b;}
inline int Max(int a, int b) {return (a > b)? a: b;}


///  Macro for 16-bits unsigned integer byte swap in assembly.
///  unTemp = (op1>>8)&0x00FF;  // Shift upper 8 bits to lower 8 bits.
///  result = ((op1<<8)&0xFF00) + unTemp; // Shift lower 8 bits to upper 8 bits.
__STATIC_FORCEINLINE uint16_t _REV16 (uint16_t op1)
{
	uint16_t result;

	__ASM volatile ("rev16 %0, %1" : "=r" (result) : "r" (op1) );
	return(result);
}


///
/// Function name		: Proce_TCM8230_Driver
///
/// Author				: Fabian Kung
///
/// Last modified		: 25 May 2020
///
/// Code Version		: 0.94
///
/// Processor			: ARM Cortex-M7 family
///
/// Processor/System Resource
/// PINS				: 1. Pin PA7 (Output) = Camera reset.
///						  2. Pin PB3 (Output) = PCK2 output, clock to camera.
///                       3. Pin PA3-PA5, PA9-PA13 (Inputs) = PIODC0 to PIODC7 (Data0-7 of camera)
///                       4. Pin PA22 (Input) = PIODCCLK (Dclk)
///                       5. Pin PA14 (Input) = PIODCEN1 (VSysnc)
///                       6. Pin PA21 (Input) = PIODCEN2 (HSync)
///
/// MODULES				: 1. TWIHS1 (High-speed Two-wire interface 1, internal) and associated driver.
///                       2. PCK0 (Programmable clock 0, internal).
///                       3. XDMAC (Extended DMA Controller, internal).
///
/// RTOS				: Ver 1 or above, round-robin scheduling.
///
/// Global Variables    : gnFrameCounter

#ifdef __OS_VER			// Check RTOS version compatibility.
#if __OS_VER < 1
#error "Proce_TCM8230_Driver: Incompatible OS version"
#endif
#else
#error "Proce_TCM8230_Driver: An RTOS is required with this function"
#endif

///
/// Description	:
/// 1. This process initializes, control and retrieves the raw digital data from
/// the CMOS camera module TCM8230 made by Toshiba Corp. in 2004.  The TCM8230MD is a 
/// VGA (640x480) color CMOS camera module.  Clock to the camera module is provided by
/// on-chip peripheral PCK0.  At present the output of PCK0 can be set to 4 MHz or
/// 8 MHz, corresponding to camera frame rate of 5 fps and 10 fps.  For default we are using
/// 10 fps.
/// 2. The pixel data from the CMOS camera is captured using the parallel capture function of
/// PIOA peripheral.
/// 3. DMA (Direct memory access) transfer is used to transfer the camera pixel data in the 
/// SRAM of the micro-controller via SAMS70 extended DMA controller (XDMAC).  
/// Here each pixel data is represented by unsigned 32 bits integer,
/// in the 2D array gunImgAtt[][] or gunImgAtt2[][].  The global variables gnImageWidth and 
/// gnImageHeight keep tracks of the width and height of each image frame.  The 2D arrays
/// gunImgAtt[][] and gunImgAtt2[][] are the so-called Frame Buffers.  We have two frame buffers
/// so that when one buffer is being filled up with new data, the data in the other buffer can
/// by accessed by image processing algorithm. Thus this driver will fill up gunImgAtt[][]
/// and gunImgAtt2[][] in an alternate fashion.
/// 4. The global variable gnFrameCounter keeps track of the number of frame captured since
/// power on.  Other process can use this information to compare difference between frames.  
/// While the flag gnValidFrameBuffer tells us which frame buffer can be access for image
/// processing tasks.
/// 5. Pre-processing of the pixels color output to extract the Luminance, Contrast, Hue and 
/// Saturation is performed here, and the result for each pixel is stored in gunImgAtt[x][y].
/// Where the x and y index denotes the pixel's location in the frame.

// User to edit these:
#define     _CAMERA_I2C2_ADD        60      // Camera I2C slave address.
#define     _CAMERA_POR_DELAY_MS    50		// Power on reset delay for camera in msec.
#define     _CAMERA_RESET_DELAY_MS  2		// Delay after disable the RESET pin in msec.

#define	PIN_CAMRESET_SET        PIOD->PIO_ODSR |= PIO_ODSR_P31		// Set camera Reset pin, PD31.
#define	PIN_CAMRESET_CLEAR		PIOD->PIO_ODSR &= ~PIO_ODSR_P31		// Clear camera Reset pin, PD31.

#define PIN_FLAG1_SET			PIOD->PIO_ODSR |= PIO_ODSR_P21;		// Set flag.  Flag 1 is used to mark the start and end of a frame.
#define PIN_FLAG1_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P21;	// Clear flag.
#define PIN_FLAG2_SET			PIOD->PIO_ODSR |= PIO_ODSR_P22;		// Set flag.  Flag 2 is used to indicate odd or even frame number.
#define PIN_FLAG2_CLEAR			PIOD->PIO_ODSR &= ~PIO_ODSR_P22;	// Clear flag.
#define PIN_FLAG3_SET			PIOA->PIO_ODSR |= PIO_ODSR_P7;					// Set flag 3.
#define PIN_FLAG3_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P7;					// Clear flag 3.
#define PIN_FLAG4_SET			PIOA->PIO_ODSR |= PIO_ODSR_P8;					// Set flag 4.
#define PIN_FLAG4_CLEAR			PIOA->PIO_ODSR &= ~PIO_ODSR_P8;					// Clear flag 4.

void Proce_TCM8230_Driver(TASK_ATTRIBUTE *ptrTask)
{
	int nTemp; 
	int nTemp2;
	unsigned int unTemp;
	static int nLineCounter = 0;
	static int nCount = 0;
	
	// Variables associated with image pre-processing.
	//int nR5, nB5;
	int nR6, nG6, nB6;
	int nLuminance;
	int ncolindex;
	int nHue;
	unsigned int unSat;
	unsigned int unMaxRGB, unMinRGB;
	int nDeltaRGB;
	int	nLuminance1, nLuminance2, nLuminance3, nLuminance4, nLuminance5, nLuminance6;
	int nLuminance7, nLuminance8;
	int	nLumGradx, nLumGrady, nLumGrad;
	static unsigned int unLumCumulative; 


	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization of I2C module, Parallel Capture and PCK1.  The PCK1 provides the clock signal	
					//           to the camera, it's frequency will determine the frame rate of the camera. The I2C module controls
					//           the camera characteristics.  Note that upon power up the camera is already in reset state by 
					//			 virtue of the all IO pins of the micro-controller is set to 0 output state.
					
					// NOTE: 9 Feb 2018 - From the preliminary datasheet of TCM8230 camera module, there is a specific power up sequence
					// to adhere to in order for the camera to initialize properly.  See page 20 of the datasheet.  Specifically we need
					// to:
					// 1) Hold the camera reset pin at low.
					// 2) Then apply 1.5V and 2.8V supplies to the camera, wait for roughly 100 msec.
					// 3) Then apply the clock pulse, wait for at least 100 clock cycles.
					// 4) Then pull camera reset pin to high, wait for at least 2000 clock cycles.
					// 5) Finally start sending I2C commands.
					// The requirements are camera dependent.  In our driver we just approximate conditions (1) and (2), as the camera VCC 
					// bus is linked to the micro-controller VCC, so essentially we power up the camera then only held the reset pin low.
					// Thus for from my observation the camera successfully initialized 90% of the time, so I will leave it at that.  In 
					// future version of the hardware to take this into consideration.
				
				PIN_CAMRESET_CLEAR;							// Assert camera RESET.	
				
				// --- The following initialization sequence for PIOA with DMA follows the recommendation of the datasheet ---
				// Parallel Capture mode interrupt settings.
				 PIOA->PIO_PCIDR = PIOA->PIO_PCIDR | PIO_PCISR_DRDY | PIO_PCIDR_ENDRX | PIO_PCIDR_DRDY | PIO_PCIDR_RXBUFF | PIO_PCIDR_OVRE; // Disable all PCM interrupts.
				
				// Setup XDMAC Channel to handle transfer of data from parallel capture buffer to memory.
				// Channel allocation: 0.
				// Source: PIOA (XDMAC_CC.PERID = 34).
				// Destination: SRAM.
				// Transfer mode (TYPE): Single block with single microblock. (BLEN = 0)
				// Chunk size (CSIZE): 1 chunks
				// Memory burst size (MBSIZE): 4
				// Channel data width (DWIDTH): halfword.
				// Source address mode (SAM): fixed.
				// Destination addres mode (DAM): increment.
				//
				// For peripheral to memory transfer, each microblock transfer is further divided into 'Chunks'.
				// The datasheet did not explain clearly the size of each chunk, I would understand it as the smallest unit of data transfer 
				// from the peripheral as set in DWIDTH in XDMAC_CC register i.e. if the smallest data unit for the peripheral is byte, 
				// then 1 chunk = 1 byte.  If the smallest data unit from the peripheral is a 16-bits half word, then 1 chunk = 1 half word. 
				// Here we will use maximum of 16 chunks per burst.  Note that the
				// number of burst should be an integral number of the microblock length.
				// 
				
				nTemp = XDMAC->XDMAC_CHID[0].XDMAC_CIS;			// Clear channel 0 interrupt status register.  This is a read-only register, reading it will clear all
																// interrupt flags.
				XDMAC->XDMAC_CHID[0].XDMAC_CSA = (uint32_t)&(PIOA->PIO_PCRHR);	// Set source start address.
				XDMAC->XDMAC_CHID[0].XDMAC_CDA = (uint32_t) gun16Pixel;	// Set destination start address.
				XDMAC->XDMAC_CHID[0].XDMAC_CUBC = XDMAC_CUBC_UBLEN(gnImageWidth); // Set the number of data chunks in a microblock.
				XDMAC->XDMAC_CHID[0].XDMAC_CC = XDMAC_CC_TYPE_PER_TRAN| 
												XDMAC_CC_CSIZE_CHK_1| 
												XDMAC_CC_MBSIZE_FOUR| 
												XDMAC_CC_DWIDTH_HALFWORD|
												XDMAC_CC_SAM_FIXED_AM| 
												XDMAC_CC_DAM_INCREMENTED_AM|
												XDMAC_CC_SIF_AHB_IF1|		// IF1 is Master AHB 5, connected to peripheral bus.
												XDMAC_CC_DIF_AHB_IF0|		// IF0 is Master AHB 4, connected to internal SRAM.
												XDMAC_CC_DSYNC_PER2MEM|
												XDMAC_CC_SWREQ_HWR_CONNECTED|
												XDMAC_CC_PERID(34); 
				
				XDMAC->XDMAC_CHID[0].XDMAC_CNDC = 0;		// Next descriptor control register.
				XDMAC->XDMAC_CHID[0].XDMAC_CBC = 0;			// Block control register.
				XDMAC->XDMAC_CHID[0].XDMAC_CDS_MSP = 0;		// Data stride memory set pattern register.
				XDMAC->XDMAC_CHID[0].XDMAC_CSUS = 0;		// Source microblock stride register.
				XDMAC->XDMAC_CHID[0].XDMAC_CDUS = 0;		// Destination microblock stride register.		
							
				// Enable parallel Capture mode of PIOA.
				// Note: 26 Nov 2015, the following sequence needs to be followed (from datasheet), setting of PCEN flag
				// should be the last.  Once PCEN is set, all the associated pins will automatically become input pins.
				// Note that upon power up, by default all pins are set to output, so momentarily there can be a high current
				// drawn from the chip.
				
				PIOA->PIO_PCMR = PIOA->PIO_PCMR | PIO_PCMR_DSIZE_HALFWORD;				// Data size is half word (16 bits).
				PIOA->PIO_PCMR = PIOA->PIO_PCMR & ~PIO_PCMR_ALWYS & ~PIO_PCMR_HALFS;	// Sample all data, sample the data only when both
																						// PIODCEN1 or PIODCEN2 are HIGH.	
				PIOA->PIO_PCMR = PIOA->PIO_PCMR | PIO_PCMR_PCEN;						// Enable Parallel Capture module.  All related pins
																						// will be automatically set to input.
																									
				gnFrameCounter = 0;														// Clear frame counter.	
				gnValidFrameBuffer = 0;													// Initially set to 2.		
				OSSetTaskContext(ptrTask, 1, 500*__NUM_SYSTEMTICK_MSEC);				// Next state = 1, timer = 500 msec.	A long delay for the voltage in
																						// the circuit to settle down.													
			break;
			
			case 1: // State 1 - Idle.			
				OSSetTaskContext(ptrTask, 2, 20*__NUM_SYSTEMTICK_MSEC);					// Next state = 2, timer = 20 msec.
			break;
			
			case 2: // State 2 - Settings of PCK2 (Programmable Clock 2, pin PB3) to generate the clock signal for the camera.

				PMC->PMC_PCK[2] = PMC_PCK_CSS_MCK | PMC_PCK_PRES(8);			// Set PCK2 pre-scaler = Value + 1, and select clock source as MCK (e.g. via PLLA/2).
																				// Output PCK2 = 16.667 MHz, frame rate = 20.83 fps.				
				//PMC->PMC_PCK[2] = PMC_PCK_CSS_MCK | PMC_PCK_PRES(9);			// Set PCK2 pre-scaler = Value + 1, and select clock source as MCK (e.g. via PLLA/2).
																				// Output PCK2 = 15 MHz, frame rate = 18.75 fps.																				
				//PMC->PMC_PCK[2] = PMC_PCK_CSS_MAIN_CLK | PMC_PCK_PRES(0);		// Set PCK2 pre-scaler = Value + 1, and select clock source as MAIN_CLK (e.g. the crystal oscillator).
																				// MAIN_CLK = 12 MHz, Prescaler = 1.  Thus output of PCK2 = 12 MHz.  Frame rate = 15 fps.
				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P3;					// Set PB3 to be controlled by Peripheral.
				PMC->PMC_SCER = PMC_SCER_PCK2;									// Enable PCK2.
				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) | PIO_ABCDSR_P3;	// Connect PB3 to Peripheral block B.
						
				/*
				// Set Programmable Clock 0 (PCK0) to:
				// Source: Main clock (12 MHz).
				// Pre-scaler = divide by 1.
				// Effective output = 12 MHz.
																				// If we haven't done so, 
																				// we need to disable the erase function before PB12 can be used as normal I/O pin.
				MATRIX->CCFG_SYSIO = (MATRIX->CCFG_SYSIO) | CCFG_SYSIO_SYSIO12;	// PB12 function selected, ERASE function disabled.
				//PMC->PMC_PCK[0] = PMC_PCK_CSS_MCK | PMC_PCK_PRES(14);			// Set PCK0 pre-scaler = Value + 1, and select clock source as MCK (e.g. via PLLA).
																				// Output PCK0 = 10 MHz, frame rate = 12.5 fps.
				PMC->PMC_PCK[0] = PMC_PCK_CSS_MAIN_CLK | PMC_PCK_PRES(0);		// Set PCK0 pre-scaler = Value + 1, and select clock source as MAIN_CLK (e.g. the crystal oscillator).
																				// MAIN_CLK = 12 MHz, Prescaler = 1.  Thus output of PCK0 = 12 MHz.  Frame rate = 15 fps.
				PIOB->PIO_PDR = (PIOB->PIO_PDR) | PIO_PDR_P12;					// Set PB12 to be controlled by Peripheral.
				PMC->PMC_SCER = PMC_SCER_PCK0;									// Enable PCK0.
				PIOB->PIO_ABCDSR[0] = (PIOB->PIO_ABCDSR[0]) | PIO_ABCDSR_P12;	// Connect PB12 to Peripheral block D.	
				PIOB->PIO_ABCDSR[1] = (PIOB->PIO_ABCDSR[1]) | PIO_ABCDSR_P12;	// 	
				*/								
				OSSetTaskContext(ptrTask, 3, 100*__NUM_SYSTEMTICK_MSEC);		// Next state = 3, timer = 100 msec.  According to TCM8230MD pre-liminary datasheet one needs at least
																				// 100 cycles of the clock before disable the RESET pin.
			break;			

			case 3: // State 3 - Disable camera reset, delay.
				PIN_CAMRESET_SET;												// Disable camera RESET.
				nCount++;														// According to TCM8230MD pre-liminary datasheet, after RESET is disabled, one needs to wait for at least
																				// 2000 clock cycles before sending I2C commands.
				OSSetTaskContext(ptrTask, 4, 200*__NUM_SYSTEMTICK_MSEC);		// Next state = 4, timer =200 msec.	
				//OSSetTaskContext(ptrTask, 5, 200*__NUM_SYSTEMTICK_MSEC);		// Next state = 5, timer =200 msec.																	
			break;
			
			case 4: // State 4 - Run multiple start-up sequence.
				if (nCount > 3)													// Run the reset sequence a few times so that the
				{																// camera can reset properly in the presence of power supply
																				// fluctuation.
					OSSetTaskContext(ptrTask, 5, 1*__NUM_SYSTEMTICK_MSEC);		// Next state = 5, timer = 1 msec.	
				}
				else
				{
					//RSTC->RSTC_CR = (RSTC->RSTC_CR) | RSTC_CR_PROCRST;			// Assert Processor reset.
					OSSetTaskContext(ptrTask, 0, 1*__NUM_SYSTEMTICK_MSEC);		// Next state = 0, timer = 1 msec.	
				}
				break;
						
			case 5: // State 5 - Set synchronization code, vertical and horizontal timing format, and picture mode.
			//nWriteSCCB(0x1E, 0x6C);                 // DMASK = 0x01.
			// HSYNCSEL = 1.
			// CODESW = 1 (Output synchronization code).
			// CODESEL = 0 (Original synchronization code format).
			// TESPIC = 1 (Enable test picture mode).
			// PICSEL = 0x00 (Test picture output is color bar).
			
			//nWriteSCCB(0x1E, 0x68);                 // DMASK = 0x01.
			// HSYNCSEL = 1.
			// CODESW = 1 (Output synchronization code).
			// CODESEL = 0 (Original synchronization code format).
			// TESPIC = 0 (Disable test picture mode).
			// PICSEL = 0x00 (Test picture output is color bar).
			//OSSetTaskContext(ptrTask, 3, 20);
				if (gI2CStat.bI2CBusy == 0)				// Make sure I2C module is not being used.
				{
					gbytI2CByteCount = 1;				// Indicate no. of bytes to transmit.
					gbytI2CRegAdd = 0x1E;				// Start address of register.
					//gbytI2CTXbuf[0] = 0x68;				// Data, normal operation.
					gbytI2CTXbuf[0] = 0x6C;				// Data, test mode, output color bar.
					//gbytI2CTXbuf[0] = 0x6D;				// Data, test mode, output color ramp 1.
					//gbytI2CTXbuf[0] = 0x6E;				// Data, test mode, output color ramp 2.
					gbytI2CSlaveAdd =  _CAMERA_I2C2_ADD;	// Camera I2C slave address.
					gI2CStat.bSend = 1;
					OSSetTaskContext(ptrTask, 6, 5*__NUM_SYSTEMTICK_MSEC);      // Next state = 6, timer = 5 msec.
				}
				else
				{
					OSSetTaskContext(ptrTask, 5, 1);		// Next state = 5, timer = 1.
				}
			break;
			
			case 6: // State 6 - Set AC frequency (ACF) of fluorescent light to 50 Hz for Malaysia.  Actually this is not important if the
					// ACFDET bit is set to AUTO (0).
					// Also maximum frame rate is set to 30 fps.
					// DCLK polarity = Normal.
				if (gI2CStat.bI2CBusy == 0)				// Make sure I2C module is not being used.
				{
					gbytI2CByteCount = 1;				// Indicate no. of bytes to transmit.
					gbytI2CRegAdd = 0x02;				// Start address of register.
					gbytI2CTXbuf[0] = 0x00;				// Data.  Max frame rate is 30 fps.
					//gbytI2CTXbuf[0] = 0x80;				// Data.  Max frame rate is 15 fps.
					gbytI2CSlaveAdd =  _CAMERA_I2C2_ADD;	// Camera I2C slave address.
					gI2CStat.bSend = 1;
					OSSetTaskContext(ptrTask, 7, 5*__NUM_SYSTEMTICK_MSEC);      // Next state = 7, timer = 5 msec.
				}
				else
				{
					OSSetTaskContext(ptrTask, 6, 1);		// Next state = 6, timer = 1.
				}
			break;

			case 7: // State 7 - Turn on camera and set output format and frame resolution.	
					// 28 Jan 2016: This step should be done last.  After the camera is turned ON, the signal lines
					// DCLK, HSYNC, VSYNC will become active.	
				// Turn on camera.
				// Enable D0-D7 outputs.
				// RGB565 format, color.
				if (gI2CStat.bI2CBusy == 0)				// Make sure I2C module is not being used.
				{
				     gbytI2CByteCount = 1;				// Indicate no. of bytes to transmit.
				     gbytI2CRegAdd = 0x03;				// Start address of register.
				     gbytI2CTXbuf[0] = 0x0E;			// Data, QQVGA(f).
					 //gbytI2CTXbuf[0] = 0x12;			// Data, QQVGA(z).
					 //gbytI2CTXbuf[0] = 0x22;			// Data, subQCIF(f).
					 //gbytI2CTXbuf[0] = 0x26;			// Data, subQCIF(z).
				     gbytI2CSlaveAdd =  _CAMERA_I2C2_ADD;	// Camera I2C slave address.
				     gI2CStat.bSend = 1;
					 OSSetTaskContext(ptrTask, 8, 5*__NUM_SYSTEMTICK_MSEC);      // Yes, next state = 8, timer = 5 msec. 
				}		
				else                                         // I2C still busy.     
				{
					OSSetTaskContext(ptrTask, 7, 1);		// Next state = 7, timer = 1.
				}
			break;
			
			case 8: // State 8 - Wait for idle condition, this is signified by:
					//           VSync (or VD) = 'H' (PA14)
					//           HSync (or HD) = 'L' (PA21)
					//           to indicate start of new frame.  So we wait for this H-to-L transition before
					//			 we enable the PDC.
				// Check pin PA14 (PIODCEN1) and PA21 (PIODCEN2) status.
				
				gnCameraReady = _CAMERA_READY;				// Indicates camera module is ready.
				
				if (((PIOA->PIO_PDSR & PIO_PDSR_P14) > 0) && ((PIOA->PIO_PDSR & PIO_PDSR_P21) == 0))
				{	// Idle condition
					
					//nTemp = XDMAC->XDMAC_CHID[0].XDMAC_CIS;			// Clear channel 0 interrupt status register.  This is a read-only register, reading it will clear all
					//XDMAC->XDMAC_CHID[0].XDMAC_CDA = (uint32_t) gn16Pixel;	// Set destination start address.
					//XDMAC->XDMAC_GE = XDMAC_GE_EN0;					// Enable XDMAC Channel 0 (Flag ST0 in XDMAC_GS will be set by hardware).
					OSSetTaskContext(ptrTask, 9, 1);		// Next state = 9, timer = 1.
				}
				else
				{	// Non-idle condition.				
					OSSetTaskContext(ptrTask, 8, 1);		// Next state = 8, timer = 1.
				} 			
			break;
			

			case 9: // State 9 - Wait for start of new frame condition, this is signified by:
					//			 Vsync = 'L'
					//			 Hsync = 'L'
					// Once start-of-frame condition is received, setup Channel 1 of XDMAC to
					// transfer Parallel Capture data to memory.
				
				// Check pin PA14 status (PIODCEN1), Vsync.
				if ((PIOA->PIO_PDSR & PIO_PDSR_P14) == 0) 
				{	// Low.			

					//PIN_FLAG1_SET;									// Set indicator flag.
					
					// Initialize XDMAC for PIOA receive operation.	
					// Note: Since this is the 1st pixel line, no need to concern about DCache coherency with SRAM data.
					nTemp = XDMAC->XDMAC_CHID[0].XDMAC_CIS;			// Clear channel 0 interrupt status register.  This is a read-only register, reading it will clear all
					XDMAC->XDMAC_CHID[0].XDMAC_CDA = (uint32_t) gun16Pixel;	// Set destination start address.	
					XDMAC->XDMAC_GE = XDMAC_GE_EN0;					// Enable XDMAC Channel 0 (Flag ST0 in XDMAC_GS will be set by hardware).							

					nLineCounter = 0;								// Reset line counter.
					for (nTemp = 0; nTemp < 128; nTemp++)			// Clear the luminance histogram array
					{												// at the start of each new frame.
						gunIHisto[nTemp] = 0;
					}					
					OSSetTaskContext(ptrTask, 10, 1);				// Next state = 10, timer = 1.
				}
				else
				{	// High.
					OSSetTaskContext(ptrTask, 9, 1);			// Next state = 9, timer = 1.					
				}
				break;			

			case 10: // State 10 - Wait until DMA transfer of 1 line of pixel data is complete.	
				//PIN_FLAG4_SET;									
				if ((XDMAC->XDMAC_GS & XDMAC_GS_ST0_Msk) == 0)		// The flag ST0 in XDMAC_GS is cleared when the transfer is complete,
																	// indicating the pixel line buffer is full.
				{	
					//PIOA->PIO_ODSR |= PIO_ODSR_P27;					// Set task indicator flag.
																	// Pixel line buffer is full.											
					nLineCounter++;									// Increment row counter.
					
					// Initialize XDMAC PIOA receive operation.
					SCB_CleanDCache();								// According to Atmel's AT17417 (Usage of XDMAC on SAMS/SAME/SAMV), to
																	// prevent memory coherency issue, we should clean data cache first before
																	// enabling DMA, then we should invalidate data cache after DMA is enabled.
																	// This is especially true when more than 1 masters is accessing the SRAM, here
																	// it is the XDMAC master and Cortex M7 master accessing SRAM simultaneously.
																	// Assuming Write-Back with Read and Write-Allocate (WB-RWA) cache policy:
																	// Cleaning the DCache writes the cache line back to SRAM.
																	// Invalidating the DCache forces subsequent read to copy the data from SRAM
																	// to the cache.
																	// Reference: TB3195, "Managing cache coherency on Cortex M7 based MCUs", Microchip Technology 2018.
																	
					nTemp = XDMAC->XDMAC_CHID[0].XDMAC_CIS;			// Clear channel 0 interrupt status register.  This is a read-only register, reading it will clear all
																	// interrupt flags.
					XDMAC->XDMAC_CHID[0].XDMAC_CDA = (uint32_t) gun16Pixel;	// Set destination start address.
					XDMAC->XDMAC_GE = XDMAC_GE_EN0;					// Enable XDMAC Channel 0 (Flag ST0 in XDMAC_GS will be set by hardware).
					SCB_InvalidateDCache();							// Mark the data cache as invalid. Subsequent read from DCache forces data to be copied from SRAM to
																	// the cache.  This is to be used after XDMAC updates the SRAM without the knowledge of the CPU's cache
																	// controller.
					
					// --- Pre-processing one line of image data here ---
					// Extract intensity.					
					for (ncolindex = 0; ncolindex < gnImageWidth; ncolindex++)
					{				
						// --- Compute the 8-bits grey scale or intensity value of each pixel and
						// update grey scale histogram ---
						//nTemp2 = gun16Pixel[ncolindex];		// First get the 16-bits pixel word from line buffer.
						
						// Assume data read into parallel bus by ARM Cortex M7 is: [Byte0][Byte1]
						// nTemp = (nTemp2 << 8) & 0xFF00;		// Swap the position of lower and upper 8 bits!
						// nTemp2 = (nTemp2 >> 8) & 0xFF;		// This is due to the way the PDC store the pixel data.  
															    // 14 Jan 2016: The upper 16 bits may not be 0! Thus need
															    // to mask off. 
						// nTemp = nTemp + nTemp2;				// Reform the 16-bits pixel data, in RGB565 format.

						// The data read into parallel bus by ARM Cortex M7 is: [Byte0][Byte1], e.g. in big endian format.
						// However, the correct order should be small endian, so we need to swap the bytes order to obtain
						// [Byte1][Byte0].
						unTemp = __REV16(gun16Pixel[ncolindex]);	// Swap the position of lower and upper 8 bits!
																	// The original method achieve this using C routines as
																	// shown above, but using inline assembly instruction 
																	// is more efficient.

						nR6 = (unTemp >> 10) & 0x3E;				// Form the 6 bits R component.
						nG6 = (unTemp >> 5) & 0x3F;					// Get the 6 bits G component.															
						nB6 = (unTemp & 0x01F)<<1;					// For the 6 bits B component. 
																							
						// --- 6 Jan 2015 ---
						// Here we approximate the luminance I (or Y) as:
						// I = 0.250R + 0.625G + 0.125B = (2R + 5G + B)/8
						// where R, G and B ranges from 0 to 255.
						// or I = (2*R + 5*G + B)>>3 for integer variables R, G, B.
						// This avoids multiplication with real number, hence speeding up the
						// computation, though with the increase in blue and decrease
						// in green components intensities, we would get a slightly darker
						// gray scale image.
						
						//	nLuminance = (2*(nR5<<3) + 5*(nG6<<2) + (nB5<<3))>>3;
						
						// Alternatively since actual values for R, G and B are 5, 6 and 5 bits respectively,
						// we can normalize R and B to 6 bits by shifting, then convert to gray scale as:
						//	nLuminance = (2*(nR5<<1) + 5*(nG6) + (nB5<<1))>>2;
						
						// Or more efficient alternative, with no multiplication:
						//	nLuminance = ((nR5<<2) + (nG6<<2) + (nB5<<1) + nG6)>>2;
						// This will result in gray scale value from 0 to 127, occupying 7 bits.
						
						if (gnLuminanceMode == 0)
						{	
							//nLuminance = ((nR5<<2) + (nG6<<2) + (nB5<<1) + nG6)>>2; // 7-bits luminance from RGB components.
							nLuminance = ((nR6<<1) + (nG6<<2) + nB6 + nG6)>>2; // 7-bits luminance from RGB components.
						}
						else if (gnLuminanceMode == 1) 
						{
							//nLuminance = nR5<<2;	// 7-bits luminance from only 5-bits Red component.
							nLuminance = nR6<<1;	// 7-bits luminance from only 6-bits Red component.
						}
						else if (gnLuminanceMode == 2)
						{
							nLuminance = nG6<<1;	// 7-bits luminance from only 6-bits Green component.					
						}
						else
						{
							//nLuminance = nB5<<2;	// 7-bits luminance from only 5-bits Blue component.
							nLuminance = nB6<<1;	// 7-bits luminance from only 6-bits Blue component.
						}				
						
						unLumCumulative = unLumCumulative + nLuminance;							// Update the sum of luminance for all pixels in the frame.
						
						gunIHisto[nLuminance]++;												// Update the intensity histogram.
						
						
						// Optional - Reduce the luminance to black and white.					
						//nLumReference = 100;									// Agreed luminance for White.
						//if (nLuminance < (gunAverageLuminance))
						//{
						//	nLuminance = 0;
						//}
						//else 
						//{
						//	nLuminance = 100;
						//}
						//											


						// --- Compute the saturation level ---
						unMaxRGB = Max(nR6, Max(nG6, nB6));     // Find the maximum of R, G or B component
						unMinRGB = Min(nR6, Min(nG6, nB6));     // Find the minimum of R, G or B components.
						nDeltaRGB = unMaxRGB - unMinRGB;
						unSat = nDeltaRGB;                         
						// Note: Here we define the saturation as the difference between the maximum and minimum RGB values.
						// A more proper term is called Chroma (as per Wikipedia article).
						// In normal usage this value needs to be normalized with respect to maximum RGB value so that
						// saturation is between 0.0 to 1.0.  Here to speed up computation we avoid using floating point
						// variables. Thus the saturation is 6 bits since the color components are 6 bits, from 0 to 63.

						// --- Compute the hue ---
						// When saturation is too low, the color is near gray scale, and hue value is not accurate.
						// Similarly when the light is too bright, the difference between color components may not be large
						// enough to work out the hue, and hue is also not accurate.  
						// From color theory (e.g. see Digital Image Processing by Gonzales and Woods, 2018), the minimum 
						// RGB component intensity corresponds to the white level intensity.  Thus the difference
						// between maximum RGB component and white level is an indication of the saturation level.
						// This difference needs to be sufficiently large for reliable hue computation.
						// For 6 bits RGB components, the maximum value of recognition, we arbitrary sets this to at least 
						// 10% of the maximum RGB component. For 6 bits RGB color (as in RGB565 format) components, max = 63
						// and min = 0.  Thus maximum difference is 63.  10% of this is 6.30. We then experiment with
						// thresholds of 2 to 7 and select the best in terms of sensitivity and accuracy for the camera.
						// Once we identified the condition where hue calculation is no valid, we need to distinguish 
						// between too bright and too dark/grayscale conditions.  For these two scenarios, we analyze the maximum
						// RGB value.  From experiment, we set the threshold at 30% or roughly 20.  Thus if 
						// saturation level is too low, we check the maximum RGB level.  If this is <= 20, then it is
						// 'No hue' due to low light condition.  Else it is 'No hue' due to too bright condition.
						
						if (nDeltaRGB < 2)              // Check if it is possible to make out the hue. 
						{	
														
							//if (unMaxRGB < 12)			// Distinguish between too bright or too dark/grayscale conditions.	Using
							//{							// maximum RGB criteria.
							//	nHue = _NO_HUE_DARK; 
							//} 
							//else
							//{
							//	nHue = _NO_HUE_BRIGHT;	
							//} 
							
							if(nLuminance < 60)			// Using luminance criteria.
							{
								nHue = _NO_HUE_DARK;
							}
							else
							{
								nHue = _NO_HUE_BRIGHT;
							}
						}
						else   // Computation of hue, here I am using the hexagonal projection method for HSV color space, 
								// as described in Wikipedia. https://en.wikipedia.org/wiki/HSL_and_HSV
							   // This is easier than circular projection which require arc cosine function.
						{
							if (nR6 == unMaxRGB)          // nR6 is maximum. Note: since we are working with integers,
							{                                          // be aware that when we perform integer division,
								nHue = (60*(nG6 - nB6))/nDeltaRGB;   // the remainder will be discarded.
							}
							else if (nG6 == unMaxRGB)     // nG6 is maximum.
							{
								nHue = 120 + (60*(nB6 - nR6))/nDeltaRGB;
							}
							else if (nB6 == unMaxRGB)     // nB6 is maximum.
							{
								nHue = 240 + (60*(nR6 - nG6))/nDeltaRGB;
							}
		                 
							if (nHue < 0)
							{
								nHue = nHue + 360;
							}
						}
						
						/* Temporay disable the Sobel Kernel operation.
						// Computing the luminance gradient using Sobel's Kernel.
						if ((ncolindex > 1) && (nLineCounter > 1))						// See notes on the derivation of this range.
						{
							// See notes. For 1st column of interest we need to read all 8 adjacent pixel luminance to compute the 
							// gradients along vertical and horizontal axis.  For subsequent columns we only need to read in the 
							// luminance values for 4 adjacent pixels.
							//  |  L1 L7 L2  ---> Columns
							//  |  L3 G  L4
							// \|/ L5 L8 L6   Where L6 = Current pixel under process, G = pixel whose gradient we are computing.
							//  Rows
							if (ncolindex == 2)
							{
								nLuminance1 = gunImgAtt[ncolindex-2][nLineCounter-2] & _LUMINANCE_MASK;
								nLuminance5 = gunImgAtt[ncolindex-2][nLineCounter] & _LUMINANCE_MASK;
								nLuminance7 = gunImgAtt[ncolindex-1][nLineCounter-2] & _LUMINANCE_MASK;
								nLuminance8 = gunImgAtt[ncolindex-1][nLineCounter] & _LUMINANCE_MASK;								
							}
															
							nLuminance2 = gunImgAtt[ncolindex][nLineCounter-2] & _LUMINANCE_MASK;	
							nLuminance3 = gunImgAtt[ncolindex-2][nLineCounter-1] & _LUMINANCE_MASK;
							nLuminance4 = gunImgAtt[ncolindex][nLineCounter-1] & _LUMINANCE_MASK;							
							//nLuminance6 = gunImgAtt[ncolindex][nLineCounter] & _LUMINANCE_MASK;	// This is the same as nLuminance.					
							nLuminance6 = nLuminance;	
					
							
							// Calculate x gradient											
							nLumGradx = nLuminance2  + nLuminance6 - nLuminance1 - nLuminance5;
							nLumGradx = nLumGradx + ((nLuminance4 - nLuminance3) << 1);
							// Calculate y gradient
							nLumGrady = nLuminance5  + nLuminance6 - nLuminance1 - nLuminance2;
							nLumGrady = nLumGradx + ((nLuminance8 - nLuminance7) << 1);

							// Shift samples to obtain current adjacent luminance values.  This is faster than reading the values from 
							// a 2D array and apply masking to extract the luminance.
							nLuminance1 = nLuminance7;
							nLuminance5 = nLuminance8;
							nLuminance7 = nLuminance2;
							nLuminance8 = nLuminance6;
							
							if (nLumGradx < 0)		// Only magnitude is required.
							{
								nLumGradx = -nLumGradx;
							}
							if (nLumGrady < 0)		// Only magnitude is required.
							{
								nLumGrady = -nLumGrady;
							}
																// Calculate the magnitude of the luminance gradient.
							nLumGrad = nLumGradx + nLumGrady;	// It should be nLumGrad = sqrt(nLumGradx^2 + nLumGrady^2)
																// Here we the approximation nLumGrad = |nLumGradx| + |nLumGrady|

							if (nLumGrad > 127)		// Limit the maximum value to 127 (7 bits only).  Bit8 is not used for
							{						// luminance indication.
								nLumGrad = 127;
							}
							if (nLumGrad < 20)		// To remove gradient noise.  Can reduce to 10 if the camera quality is good.
							{
								nLumGrad = 0;
							}
						}
						else
						{
							nLumGrad = 0;
						} 
						*/
						
						// Update the pixel attributes.
														
						// Consolidate all the pixel attribute into the attribute array.
						if ((gnFrameCounter & 0x00000001) == 1)			// If frame number is odd.
						{												// processes.  So we should update gunImgAtt2[].
							gunImgAtt2[ncolindex][nLineCounter] = nLuminance | (unSat << _SAT_SHIFT) | (nHue << _HUE_SHIFT);
							gunImgAtt2[ncolindex-1][nLineCounter-1] = gunImgAtt2[ncolindex-1][nLineCounter-1] | (nLumGrad << _GRAD_SHIFT);					
						}
						else                                            // Frame number is even.
						{	
																		// Data in gunImgAtt2[] is being accessed by other processes.																		
							gunImgAtt[ncolindex][nLineCounter] = nLuminance | (unSat << _SAT_SHIFT) | (nHue << _HUE_SHIFT);
							gunImgAtt[ncolindex-1][nLineCounter-1] = gunImgAtt[ncolindex-1][nLineCounter-1] | (nLumGrad << _GRAD_SHIFT);
						}

					} // for (ncolindex = 0; ncolindex < gnImageWidth; ncolindex++)				
					
					// --- End of pre-processing one line of image data here ---
											
				}
				else
				{
					
				}  // if ((XDMAC->XDMAC_GS & XDMAC_GS_ST0_Msk) == 0)
								
				
				if (nLineCounter == gnImageHeight)				// Check for end of frame.
				{					
					//gnFrameCounter++;							// Update frame counter.
					//PIN_FLAG1_CLEAR;							// Clear indicator flag.
					OSSetTaskContext(ptrTask, 11, 1);			// Next state = 11, timer = 1.
				}	
				else
				{						
																// Not end of frame yet, continue to receive next 
																// line of pixel data.
					OSSetTaskContext(ptrTask, 10, 1);			// Next state = 10, timer = 1.
				}		
				//PIN_FLAG4_CLEAR;				
			break;

			case 11: // State 11 - End, do some tidy-up chores, update frame counter and valid frame buffer flag.
				if ((gnFrameCounter & 0x00000001) == 1)			// If frame number is odd.
				{
					gnValidFrameBuffer = 1;					    // Indicating odd frame number has been updated.  Thus image processing
																// algorithm should get the pixel attributes from Frame Buffer 2.					
				}
				else
				{
					gnValidFrameBuffer = 0;						// Indicating even frame number has been updated.  Thus image processing
																// algorithm should get the pixel attributes from Frame Buffer 1.					
				}
				gnFrameCounter++;								// Update frame counter.
				gunAverageLuminance = unLumCumulative/_NOPIXELSINFRAME;	// Update the average Luminance for current frame.
				//PIN_FLAG1_CLEAR;								// Clear indicator flag.
				unLumCumulative = 0;							// Reset the sum of cumulative Luminance.
				OSSetTaskContext(ptrTask, 8, 1);				// Next state = 8, timer = 1.
			break;
			/*
			case 12: // State 12 - Power down sequence, disable camera clock.
				PMC->PMC_SCDR = PMC_SCDR_PCK2;					// Disable PCK2.
				OSSetTaskContext(ptrTask, 13, 1*__NUM_SYSTEMTICK_MSEC);				// Next state = 13, timer = 1 msec.
			break;
			
			case 13: // State 13 - Idle.
				OSSetTaskContext(ptrTask, 14, 1);				// Next state = 14, timer = 1.
			break;
			
			case 14: // State 14 - Idle.
				gnCameraLED = 0;								// Shut down the camera LED.
				OSSetTaskContext(ptrTask, 14, 1);				// Next state = 14, timer = 1.
			break;
			*/
			default:
				OSSetTaskContext(ptrTask, 0, 1);				// Back to state = 0, timer = 1.
			break;
		}
	}
}


///
/// Process name		: Proce_Camera_LED_Driver
///
/// Author              : Fabian Kung
///
/// Last modified		: 22 August 2018
///
/// Code Version		: 0.95
///
/// Processor			: ARM Cortex-M4 family
///
/// Processor/System Resources
/// PINS                : Pin PD0 (Output) = Camera LED control, active high.
///
/// MODULES             :
///
/// RTOS                : Ver 1 or above, round-robin scheduling.
///
/// Global variables    : gnCameraLED

#ifdef 				  __OS_VER			// Check RTOS version compatibility.
#if 			  __OS_VER < 1
#error "Proce_Camera_LED_Driver: Incompatible OS version"
#endif
#else
#error "Proce_Camera_LED_Driver: An RTOS is required with this function"
#endif

/// Description: Process to control the eye LEDs and camera LED.
/// Usage:
/// To light up the visible light LED, set gnCameraLED from 1 to 7 (7 being the brightest).  Value
/// greater than 7 will be capped to 7 by the driver.
/// Setting gnCameraLED to 0 or smaller turn off the LED. Setting the respective LED control
///

#define	PIN_CAMLED_ON		PIOD->PIO_ODSR |= PIO_ODSR_P0			// Turn on camera LED, PD0.
#define	PIN_CAMLED_OFF		PIOD->PIO_ODSR &= ~PIO_ODSR_P0			// Turn off camera LED, PD0.

void Proce_Camera_LED_Driver(TASK_ATTRIBUTE *ptrTask)
{
	static	int	nCounter = 0;

	if (ptrTask->nTimer == 0)
	{
		switch (ptrTask->nState)
		{
			case 0: // State 0 - Initialization.
			PIN_CAMLED_OFF;							// Off all LEDs first.
			nCounter = 0;
			gnCameraLED = 0;
			OSSetTaskContext(ptrTask, 1, 1000*__NUM_SYSTEMTICK_MSEC);    // Next state = 1, timer = 1000 msec.
			break;

			case 1: // State 1 - Wait for camera module to complete initialization sequence.
			if (gnCameraReady == _CAMERA_READY)
			{
				OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
			}
			else
			{
				OSSetTaskContext(ptrTask, 1, 1);    // Next state = 1, timer = 1.
			}
			break;

			case 2: // State 2 - Drive the camera and eye LEDs.
			if ((gnCameraLED & 0x0007) > nCounter)	// Get last 3 bits and compare with counter.
			{
				PIN_CAMLED_ON;
			}
			else
			{
				PIN_CAMLED_OFF;
			}
			nCounter++;
			if (nCounter == 7)
			{
				nCounter = 0;
			}
			OSSetTaskContext(ptrTask, 2, 1);    // Next state = 2, timer = 1.
			break;
			
			default:
			OSSetTaskContext(ptrTask, 0, 1); // Back to state = 0, timer = 1.
			break;
		}
	}
}