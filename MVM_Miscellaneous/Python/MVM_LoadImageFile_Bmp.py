# -*- coding: utf-8 -*-
"""
Created on Sat Jan 25 11:42:21 2020

@author: User
"""
import matplotlib.pyplot as plt

#Set the width and height of the image in pixels.
_imgwidth = 160
_imgheight = 120

#Load the BMP image, the image contains RGB channels, but here it is a 
#grayscale image so all R, G and B channels have similar values of
#between 0-255.
print("Loading image...")
image = plt.imread('Img.bmp',format = 'BMP')

print("Shape of image is ", image.shape)
#Extract only 1 channel of the pixel, since all RGB channels have the same
#values.
image1 = image[0:_imgheight,0:_imgwidth,0]
plt.imshow(image1,cmap='gray')
