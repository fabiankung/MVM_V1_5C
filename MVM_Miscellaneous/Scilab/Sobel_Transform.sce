// Author           : Fabian Kung
// Last modified    : 7 Feb 2018
// Purpose          : Basic code to load a 8-bit grayscale image from file

clear;
ImageWidth = 160;               // Set the size of the image. QQVGA.
// --- Import a grayscale image file and store in 2D matrix ---
ImageHeigth = 120;
HImageOriginal = scf(1);                // Set the handle to graphic window for the original
                                        // image.
path = cd("C:\FabianKung");             // Path for the image file.
Hfile = mopen("testimage.txt",'rb');    // Open a text file for reading 
                                        // (don't skip 0x0D, newline character)
M = zeros(ImageHeigth+1,ImageWidth+1);  // Matrix to hold the gray scale image data
Mt = zeros(ImageWidth+1,ImageHeigth+1); // Another matrix also to hold the gray scale image data.
                                        // 't' indicate transpose.
for i=1:ImageHeigth-1               // ImageHeigth-1 due to the last line is not
                                    // exported from the camera monitor software
    for j=1:ImageWidth
        M(i,j) = mget(1,'c',Hfile); // Read 1 pixel data, convert to double.
    end
    mgeti(1,'c',Hfile);             // Read the newline/carriage return character.
end

for i=1:ImageHeigth-1               // Transpose and flip the image so that
    for j=1:ImageWidth              // it appear at the correct orientation.
       Mt(j,ImageHeigth-i) = M(i,j) // Here the format is:
    end                             // /\ Row
end                                 // |
                                    // |
                                    // |
                                    // ---------------------> Column
                                    
mclose(Hfile);                      // Release file handle.

//Hgraf.color_map = graycolormap(127);    // Set current graphic window color map 
HImageOriginal.color_map = graycolormap(255);    //to gray scale, 127 or 255 levels.
row = 1:ImageWidth + 1;
col = 1:ImageHeigth + 1;
grayplot(row,col,Mt);               // Plot the transpose and flip image.

// --- Sobel Transform ---
HImageSobel = scf(2);               // Handle to graphic window after Sobel Transform.
MSobel = zeros(ImageWidth+1,ImageHeigth+1); // Matrix to hold the output after Sobel Transform.
for i=2:ImageHeigth-2               // Only perform the transform in the interior       
    for j=2:ImageWidth-1            // pixels.  
        Lu1 = Mt(j-1,i+1);
        Lu2 = Mt(j+1,i+1);
        Lu3 = Mt(j-1,i);
        Lu4 = Mt(j+1,i);
        Lu5 = Mt(j-1,i-1);
        Lu6 = Mt(j+1,i-1);
        Lu7 = Mt(j,i+1);
        Lu8 = Mt(j,i-1);
        DIx = Lu2 + Lu6 - Lu1 - Lu5 + 0.5*(Lu4 - Lu3);
        DIy = Lu5 + Lu6 - Lu1 - Lu2 + 0.5*(Lu8 - Lu7);
        MSobel(j,i) = abs(DIx) + abs(DIy);
    end                             
end   

HImageSobel.color_map = graycolormap(255);
row = 1:ImageWidth + 1;
col = 1:ImageHeigth + 1;
grayplot(row,col,MSobel);
