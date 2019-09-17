//////////////////////////////////////////////////////////////////////////////////////////////
//
// File				: Drivers_TCM8230.c
// Author(s)		: Fabian Kung
// Last modified	: 1 Aug 2018
// Tool-suites		: Atmel Studio 7.0 or later
//                    GCC C-Compiler
//////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DRIVER_TCM8230_SAMS70_H
#define _DRIVER_TCM8230_SAMS70_H

// Include common header to all drivers and sources.  Here absolute path is used.
// To edit if one change folder
#include "osmain.h"

//
// --- PUBLIC CONSTANTS ---
//
#define _IMAGE_HRESOLUTION   160		// For 160x120 pixels QQVGA resolution.
#define _IMAGE_VRESOLUTION   120
#define _NOPIXELSINFRAME	 19200

//
// --- PUBLIC VARIABLES ---
//

extern	int		gnFrameCounter;
extern	int		gnImageWidth;
extern	int		gnImageHeight;
extern	unsigned	int		gnCameraLED;
extern	int		gnLuminanceMode;	// Option to set how luminance value for each pixel
									// is computed from the RGB component.
									// 0 - Luminance (I) is computed from RGB using
									//	   I = 0.250R + 0.625G + 0.125B = (2R + 5G + B)/8
									// 1 - I = 4R
									// 2 - I = 2G
									// Else - I = 4B

extern	unsigned int gunImgAtt[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION];
extern	unsigned int gunImgAtt2[_IMAGE_HRESOLUTION][_IMAGE_VRESOLUTION];
extern	int		gnValidFrameBuffer;		// If equals 1, it means gunImgAtt[] data can be used for image processing.
// If equals 2, then gunImgAtt2[] data can be used instead for image processing.
// Under normal operation, the value of gnValidFrameBuffer will alternate
// between 1 and 2 as raw pixel data captured from the camera is store in
// gunImgAtt[] and gunImgAtt2[] alternately.

// bit6 - bit0 = Luminance information, 7-bits.
// bit7 - Marker flag, set to 1 if we wish the external/remote display to highlight
//        this pixel.
// bit16 - bit8 = Hue information, 9-bits, 0 to 360, No hue (gray scale) = 511.
// bit22 - bit17 = Saturation, 6-bits, 0-63 (63 = pure spectral).
// bit30 - bit23 = Luminance gradient, 8 - bits.
// bit31 - Special flag.
#define     _LUMINANCE_MASK     0x0000007F  // luminance mask, bit6-0.
#define     _CLUMINANCE_MASK    0xFFFFFF00  // One's complement of luminance mask.
#define     _LUMINANCE_SHIFT    0
#define     _HUE_MASK           0x0001FF00  // bit16-8
#define     _HUE_SHIFT          8
#define     _SAT_MASK           0x003E0000  // Saturation mask, bit22-17.
#define     _CSAT_MASK          0xFFC1FFFF  // One's complement of saturation mask.
#define     _SAT_SHIFT          17
#define     _NO_HUE_BRIGHT      420         // Value for no hue when object is too bright or near grayscale.
#define     _NO_HUE_DARK		400         // Value for no hue when object is too dark.
// Valid hue ranges from 0 to 360.
#define     _GRAD_MASK          0x7F800000  // bit30-23
#define     _CGRAD_MASK         0x807FFFFF  // One's complement of gradient mask.
#define     _GRAD_SHIFT         23
#define     _MAX_GRADIENT       255

extern	int16_t gunIHisto[255];
extern	unsigned int	gunAverageLuminance;
#define		_CAMERA_READY			1
#define		_CAMERA_NOT_READY		0
extern int				gnCameraReady;	


//
// --- PUBLIC FUNCTION PROTOTYPE ---
//
void Proce_TCM8230_Driver(TASK_ATTRIBUTE *);
void Proce_Camera_LED_Driver(TASK_ATTRIBUTE *);

#endif
