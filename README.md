# MVM_V1_5C
Contains the documentation, firmware and PC software (Windows) for machine vision module V1.5 Revision C. 
Also contains some miscellaneous files, for instance the Scilab codes to read the raw binary image file output by the PC software. 
For further information:
1. Please visit: https://fkeng.blogspot.com/2016/01/machine-vision-module.html
2. Please read the Quick Start guide in this repository.

Firmware Folders:
1. MVM_Sample_Firmware_R0.9 - Original firmware, support streaming of black-and-white images from the camera to a computer (or Raspberry Pi). Requires USB-to-Serial converter or HC-05. Codes contain sample image processing algorithm that search for brightest pixel in the image.
2. MVM_Sample_Firmware_R0.95_CNN - Same as R0.9, but with a sample routine showing incorporation of CNN (convolutional neural network) image processing algorithm. 
3. MVM_Sample_Firmware_R0.9LCD - Version support external 320x240 TFT LCD display (ILI9341 LCD controller) from Adafruit.  Streaming of image to computer is disabled.
4. MVM_Sample_Firmware_R0.50_WiFi - Experimental codes that support streaming images via WiFi connectivity, using ESP8266 module. Uses TCP protocol.
