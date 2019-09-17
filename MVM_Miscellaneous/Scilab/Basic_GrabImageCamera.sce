// Author           : Fabian Kung
// Last modified    : 29 Oct 2016
// Purpose          : Basic code to load a 8-bit grayscale image from file

clear;
ImageWidth = 160;               // Set the size of the image. QQVGA.
ImageHeigth = 120;
Hgraf = scf();                      // Get the handle to current graphic window.
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
    end                             // ---------------------> Column
end                                 // |
                                    // |
                                    // |
                                    // V
                                    // Row
                                    
mclose(Hfile);                      // Release file handle.

//Hgraf.color_map = graycolormap(127);    // Set current graphic window color map 
Hgraf.color_map = graycolormap(255);    //to gray scale, 127 or 255 levels.
row = 1:ImageWidth + 1;
col = 1:ImageHeigth + 1;
grayplot(row,col,Mt);               // Plot the transpose and flip image.


