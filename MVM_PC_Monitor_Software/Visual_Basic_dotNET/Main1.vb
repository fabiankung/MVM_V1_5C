Imports System.Threading
Imports System.IO
Imports System.Drawing.Imaging

'Description:
'This application uses multi-threading to buffer and display image data from a remote device.
'The remote device communicates with this application via COM port.  The COM port can be a 
'virtual COM port from USB or Bluetooth device or an actual physical COM port device.
'To improve efficiency, one thread will buffer the image data line-by-line while another thread
'display the image data onto the window (in a picturebox in this case).  The image data is
'trasmitted line-by-line by the remote device.  To prevent conflict, we have two line buffers, so 
'that when one is being updated with fresh data from the COM port, the other is being accessed to
'updated the image on the picturebox.
'
'
Public Class Main

    'Global public datatypes declaration 
    Public Const mstrSoftwareTitle = "Machine Vision Monitor - "
    Public Const mstrSoftwareVersion = "Version 1.02"
    Public Const mstrSoftwareAuthor = "Fabian Kung Wai Lee"
    Public Const mstrSoftwareDate = "16 Sep 2019"
    Public Const mstrCompany = "Multimedia University"
    Public Const mstrAddress = "Faculty of Engineering, Jalan Multimedia, Cyberjaya, Selangor, MALAYSIA"
    Public Const mstrWebpage = "http://foe.mmu.edu.my"

    Dim nTemp As Integer
    Dim mstrComPortName As String
    Dim mnRGB565Pixel As Integer

    'Bitmap image global variables
    Dim mbytRxData1(0 To 330) As Byte           'Buffer1 for storing 1 line of bitmap pixel data.
    'Dim mbytRxData2(0 To 330) As Byte           'Buffer2 for storing 1 line of bitmap pixel data.
    Dim mblnLine1Mutex As Boolean               'Buffer1 mutex. True = Buffer1 is used by buffering thread.
    '                                           'False = Buffer1 is being accessed by bitmap update thread.
    Dim mblnLine2Mutex As Boolean               'Buffer2 mutex. True = Buffer2 is used by buffering thread.
    'False = Buffer2 is being accessed by bitmap update thread.
    Dim mintCurrentBMPLine1Counter As Integer
    Dim mintCurrentBMPLine2Counter As Integer
    Dim mintDataPayload1Length As Integer
    Dim mintDataPayload2Length As Integer
    Dim mbytTxData(0 To 8) As Byte
    Dim mintOption As Integer

    Dim mintWatchDogCounter As Integer

    Dim mintImageWidth As Integer = 160
    Dim mintImageHeight As Integer = 120
    Dim mintImageScale As Integer = 2         '2 - The displayed image will be 2X the actual size.
    '1 - This will display the image in original size (1x).

    'Threads, here Managed Threading (.NET 4 and onwards) is used to make execution more efficient. 
    Dim t1 As New Thread(AddressOf ThreadProc1) 'Thread to buffer serial data from remote devices.

    'Graphics objects

    Private mptBmpStart As Point    'Position of bitmap (the top left hand corner).
    Private mnColor As Integer

    Private mbytPixel(mintImageWidth, mintImageHeight) As Byte '2D array to store image secondary data.

    Private mbytPixel2(mintImageWidth, mintImageHeight) As Byte '2D array to store image data.
    Private mbmpMainBMP As Bitmap   'Main bitmap display for image
    Private mrecMainBMPArea As Rectangle                        'A rectangle, specifying mbpmMainBMP properties.
    Private mbitmapdataBMPArea As BitmapData

    Private mptSecBmpStart As Point     'Position of bitmap (the top left hand corner).
    Private mintYCrossHair As Integer      'Y coordinate of horizontal line (to be superimposed into image for indication purpose)
    Private mintXCrossHair As Integer      'X coordinate of vertical line (to be superimposed into image for indication purpose)

    Private mgraphicObject As Graphics  'Global graphic object for used in displayBitmap function.

    'Auxiliary information to be shown together with the image.
    Dim mstrMessage As String
    Dim mnData1 As Integer
    Dim mnData2 As Integer
    Dim mnData3 As Integer
    Dim mnData4 As Integer
    Dim mnDataDebug As Integer
    Dim mnDebug As Integer      'Used for general purpose debugging.  This integer can be assigned
    'to internal state and display on the main window during execution
    'to debug the program
    Dim mnDataIPA1 As Integer
    Dim mnDataIPA2 As Integer

    'Private mFilePath As String = "C:\FabianKung\Testimage.txt"
    Private mFilePath As String = ""

    Private Const mnRedMask As Integer = &HF800
    Private Const mnGreenMask As Integer = &H7E0
    Private Const mnBlueMask As Integer = &H1F

    Private Const mnLuminanceRGB As Integer = 0
    Private Const mnLuminanceR As Integer = 1
    Private Const mnLuminanceG As Integer = 2
    Private Const mnLuminanceB As Integer = 3
    Private Const mnLuminanceGradient As Integer = 4
    Private Const mnHue As Integer = 5
    Private Const mnIPResult As Integer = 6

    Private Sub Main_Load(sender As Object, e As EventArgs) Handles Me.Load

        Me.Text = mstrSoftwareTitle & " " & mstrSoftwareVersion

        StatusStripLabel1.Text = "No COM Port open"

        'Initialize main picture box and graphic objects.
        mbmpMainBMP = New Bitmap(mintImageWidth + 1, mintImageHeight + 1, PixelFormat.Format24bppRgb)      'Create a bitmap for the received image.
        mptBmpStart = New Point(164, 20)        'Start position for bitmap (upper left corner).
        mrecMainBMPArea.X = 0                   'A rectangle area within the bitmap.  This will be used by the displayBitmap subroutine.
        mrecMainBMPArea.Y = 0                   'Here the area is equivalent to a row on the bitmap, thus height = 1.
        mrecMainBMPArea.Width = mbmpMainBMP.Width
        mrecMainBMPArea.Height = 1 '1 pixel

        If mintImageScale = 2 Then               'This will display the image at 2x original size.
            mbmpMainBMP.SetResolution(50, 50)    'Set resolution to 50 dots per inch.
            'If we set to 100 dots per inch, this will result in smaller image size.
        End If

        'Initialize Controls
        RadioButtonMarkerRed.Checked = True
        mnColor = 1
        ButtonClose.Enabled = False
        ButtonOpen.Enabled = True
        ButtonStart.Enabled = False

        'Initialize Option
        RadioButtonOptionLuminanceRGB.Checked = True
        mintOption = 0

        'List all available serial port in the serial port Combobox items and initialize the selection.
        For Each strSerialPortName As String In My.Computer.Ports.SerialPortNames
            ComboBoxCommPort.Items.Add(strSerialPortName)
        Next

        mstrComPortName = ComboBoxCommPort.Items(0)     'The default COM Port name.
        ComboBoxCommPort.SelectedIndex = 0

        'Initialize Threads parameters
        t1.Name = "RobotEyeMon Buffer"

    End Sub

    Private Sub Main_FormClosing(sender As Object, e As FormClosingEventArgs) Handles Me.FormClosing

        If (t1.IsAlive) Then     'Abort the secondary thread if it is still running.
            t1.Abort()
            't1.Join()
        End If

        If (SerialPort1.IsOpen = True) Then 'Close the COM port if it is still open.
            SerialPort1.Close()
        End If

    End Sub

    Private Sub ComboBoxCommPort_SelectedIndexChanged(sender As Object, e As EventArgs) Handles ComboBoxCommPort.SelectedIndexChanged

        mstrComPortName = ComboBoxCommPort.Items(ComboBoxCommPort.SelectedIndex)     'Update the COM Port name.

    End Sub

    Private Sub ButtonClose_Click(sender As Object, e As EventArgs) Handles ButtonClose.Click

        If (t1.IsAlive) Then     'Abort the secondary thread if it is still running.
            t1.Abort()
        End If
        'If (t2.IsAlive) Then    'Abort the secondary thread if it is still running.
        't2.Abort()
        'End If
        If SerialPort1.IsOpen = True Then
            SerialPort1.Close()
            StatusStripLabel1.Text = "COM Port Closed!"
            ButtonOpen.Enabled = True
            ButtonStart.Enabled = False
            TimerWatchdog.Enabled = False       'Disable watchdog timer.
        End If

    End Sub

    Private Sub ButtonOpen_Click(sender As Object, e As EventArgs) Handles ButtonOpen.Click
        Try

            If SerialPort1.IsOpen = False Then
                SerialPort1.PortName = mstrComPortName
                SerialPort1.Parity = IO.Ports.Parity.None
                SerialPort1.BaudRate = 115200
                'SerialPort1.BaudRate = 230400
                'SerialPort1.BaudRate = 460800
                SerialPort1.ReceivedBytesThreshold = 1
                SerialPort1.ReadTimeout = 500
                SerialPort1.WriteTimeout = 500
                SerialPort1.Open()
                SerialPort1.DiscardInBuffer() 'Flush buffer after initialization.

                If SerialPort1.IsOpen = True Then
                    ButtonOpen.Enabled = False
                    ButtonStart.Enabled = True
                    ButtonClose.Enabled = True
                End If

                StatusStripLabel1.Text = mstrComPortName & " Port Opened!"
            End If

        Catch ex As Exception
            MessageBox.Show(ex.Message, "ERROR", MessageBoxButtons.OK)
        End Try

    End Sub

    Private Sub ButtonStart_Click(sender As Object, e As EventArgs) Handles ButtonStart.Click
        Dim nXindex As Integer
        Dim nYindex As Integer

        Try
            If SerialPort1.IsOpen = True Then
                If mintOption = 0 Then
                    mbytTxData(0) = Asc("L")   'Ask remote unit to send luminance data of pixel (RGB)
                ElseIf mintOption = 1 Then
                    mbytTxData(0) = Asc("R")   'Ask remote unit to send luminance data of pixel (R only)
                ElseIf mintOption = 2 Then
                    mbytTxData(0) = Asc("G")   'Ask remote unit to send luminance data of pixel (G only)
                ElseIf mintOption = 3 Then
                    mbytTxData(0) = Asc("B")   'Ask remote unit to send luminance data of pixel (B only)
                Else
                    mbytTxData(0) = Asc("D")   'Ask remote unit to send luminance gradient data of pixel
                End If

                For nYindex = 0 To 119          'Initialize the secondary display buffer (for storing marker and ROI
                    For nXindex = 0 To 159      'information)
                        mbytPixel(nXindex, nYindex) = 0
                    Next
                Next
                mintYCrossHair = 60                        'Set horizontal marker line location
                mintXCrossHair = 80                        'Set vertical marker line location
                For nXindex = 0 To mintImageWidth - 1   'Draw a Red color cross-hair that went through the center of the image frame.
                    mbytPixel(nXindex, mintYCrossHair) = mnColor  'Initialize the horizontal line markers.
                Next
                For nYindex = 0 To mintImageHeight - 1
                    mbytPixel(mintXCrossHair, nYindex) = mnColor  'Initialize the vertical line markers.
                Next

                mbytTxData(1) = 0
                SerialPort1.Write(mbytTxData, 0, 1) 'Send a command byte to remote client to start the process
                SerialPort1.DiscardInBuffer()       'Flush buffer to prepare for incoming data.

                TimerWatchdog.Interval = 40         '40  msec to timeout.
                TimerWatchdog.Enabled = True        'Enable watchdog timer.
                mintWatchDogCounter = 0             'Reset watchdog counter.

                If t1.IsAlive = False Then          'July 2016: Once a thread is started, we cannot start it a 2nd time,
                    'else this would cause an exception.
                    t1 = New Thread(AddressOf ThreadProc1)
                    t1.Start()                      'Start the 1st thread if it is not running.
                End If

                mgraphicObject = Me.CreateGraphics
            Else
                StatusStripLabel1.Text = "Please open a COM Port first!"
            End If

        Catch ex As Exception
            MessageBox.Show("Start_Button: " & ex.Message, "ERROR", MessageBoxButtons.OK)
        End Try
    End Sub


    Private Sub SerialPort1_ErrorReceived(sender As Object, e As IO.Ports.SerialErrorReceivedEventArgs) Handles SerialPort1.ErrorReceived

        SerialPort1.DiscardInBuffer() 'Flush buffer to prepare for incoming data.
        StatusStripLabel1.Text = "Serial Port Error!"

    End Sub

    Private Sub RadioButtonMarkerRed_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonMarkerRed.CheckedChanged

        mnColor = 1

    End Sub

    Private Sub RadioButtonMarkerGreen_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonMarkerGreen.CheckedChanged

        mnColor = 2

    End Sub

    Private Sub RadioButtonMarkerBlue_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonMarkerBlue.CheckedChanged

        mnColor = 3

    End Sub

    Private Sub RadioButtonMarkerYellow_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonMarkerYellow.CheckedChanged

        mnColor = 4

    End Sub

    'TimerWatchdog overflow event handler.
    'This process is used to update the respective text labels on the main window.

    Private Sub TimerWatchdog_Tick(sender As Object, e As EventArgs) Handles TimerWatchdog.Tick
        Static Dim nState As Integer = 0

        Try
            LabelData1.Text = CStr(mnData1)
            LabelData2.Text = CStr(mnData2)
            LabelData3.Text = CStr(mnData3)
            LabelData4.Text = CStr(mnData4)
            LabelData5.Text = CStr(SerialPort1.BytesToRead)
            LabelData6.Text = CStr(mintYCrossHair)
            LabelData7.Text = CStr(mintXCrossHair)
            LabelDataDebug.Text = CStr(mnDataDebug)
            LabelIPA1.Text = CStr(mnDataIPA1)
            LabelIPA2.Text = CStr(mnDataIPA2)

            mintWatchDogCounter = mintWatchDogCounter + 1

        Catch ex As Exception
            MessageBox.Show("TimerWatchdog_Tick: " & ex.Message, "ERROR", MessageBoxButtons.OK)
        End Try

    End Sub

    Private Sub RadioButtonOptionLuminance_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionLuminanceRGB.CheckedChanged

        mintOption = mnLuminanceRGB

    End Sub

    Private Sub RadioButtonOptionGradient_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionGradient.CheckedChanged

        mintOption = mnLuminanceGradient

    End Sub

    Private Sub RadioButtonOptionLuminanceR_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionLuminanceR.CheckedChanged

        mintOption = mnLuminanceR

    End Sub

    Private Sub RadioButtonOptionLuminanceG_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionLuminanceG.CheckedChanged

        mintOption = mnLuminanceG

    End Sub

    Private Sub RadioButtonOptionLuminanceB_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionLuminanceB.CheckedChanged

        mintOption = mnLuminanceB

    End Sub

    Private Sub RadioButtonOptionHue_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionHue.CheckedChanged

        mintOption = mnHue

    End Sub

    Private Sub RadioButtonOptionIPResult_CheckedChanged(sender As Object, e As EventArgs) Handles RadioButtonOptionIPResult.CheckedChanged

        mintOption = mnIPResult

    End Sub

    'Last updated:  14 Aug 2019
    'Author:        Fabian Kung
    'Subroutine to update the bitmap image one line at a time.  Also superimpose markers or ROI.
    '
    '--- 1. Format for 1 line of pixel data ---
    'The image data Is send to the remote display line-by-line, using a simple RLE (Run-Length 
    'Encoding) compression format. The data format:
    'Byte0: 0xFF (Indicate start-of-line)
    'Byte1: Line number, 0-253, indicate the line number In the bitmap image.
    '       if Byte1 = 254, it indicate the following bytes are secondary data.  
    'Byte2: The length Of the data payload, excluding Byte0-2.
    'Byte3-ByteN: Data payload.
    'The data payload only accepts 7 bits value, from bit0-bit6.  Bit7 Is used to indicate
    'repetition in the RLE algorithm, the example below elaborate further.

    'Example
    'Consider the following byte stream:
    '[0xFF][3][7][90][80][70][0x83][60][0x82][120]
    'Byte2 = 7 indicates that there are 6 data bytes in this data packet.
    'Byte3 = 90 represent the 1st pixel data in line 3.  This value can represent the 
    '         luminance value of other user define data.
    'Byte4 = 80 another pixel data.
    'Byte5 = 70 another pixel data.
    'Byte6 = 0x83 = 0b10000000 + 3.  This indicates the last byte, e.g. Byte5=70 Is to
    '         be repeated 3 times, thus there are a total of 4 pixels with value of 70
    '         after pixel with value of 80. 
    'Byte7 = 60 another pixel data.
    'Byte8 = 0x82 = 0b10000000 + 2.  The pixel with value of 60 Is to be repeated 2 times.
    'Byte9 = 120, another pixel.
    '
    'The complete line of pixels Is as follows:
    'Line3 [90][80][70][70][70][70][60][60][60][120]
    '
    'This subroutine must thus be able to decode the above format and setup the bitmap pixels
    'accordingly
    'The maximumum no. of repetition: Currently the maximum no. Of repetition Is 63.  If 
    'there are 64 consequtive pixels with values of 70, we would write [70][0x80 + 0x3F]
    'Or [70][0x10111111].  The '0' at bit 6 is to prevention a repetition code of 0xFF, which
    'can be confused with start-of-line value.
    '
    '
    ' --- 2. Markers/ROI ---
    ' We can ovellap marker or ROI (region of interest) onto the bitmap image.
    ' A 2D array mbytPixel(x,y) is used to keep track of the shape and size of markers or ROI that 
    ' would be superimposed onto the bitmap image.  
    ' Whenever the value of the element in mbytPixel(x,y) is <> 0, the pixel in the bitmap will 
    ' be colored according to the value in mbytPixel(x,y).  If it is 0, the color of the bitmap
    ' will be determined by the image pixel data received from the external module.
    Private Sub DisplayBitmap()

        Static Dim nR8 As Integer
        Static Dim nHue As Integer
        Dim nData As Integer
        Dim nRepetition As Integer
        Dim nX As Integer
        Dim nY As Integer
        Dim nIndex As Integer
        Dim nIndex2 As Integer
        Dim nXCoor As Integer
        Dim nYCoor As Integer
        'Dim graphicObject As Graphics
        Dim ptrBitmap As IntPtr
        Dim nPixelSize As Integer
        Dim nPixelIndex As Integer
        Dim bytes As Integer
        Static Dim nBitmapLocked As Integer = 0
        Static Dim nColorR As Integer       'RGB components for pixel color.
        Static Dim nColorG As Integer
        Static Dim nColorB As Integer
        Static Dim nColormR As Integer      'RGB components for marker color.
        Static Dim nColormG As Integer
        Static Dim nColormB As Integer

        Try

            nY = mintCurrentBMPLine1Counter + 1  'Get line number, add 1 since the value starts from 0.  The bitmap coordinate index starts from 1.

            If (nY <= mintImageHeight) Then         'Make sure line no. is less than image height in pixels.

                'window coordinate starts from 1.
                nYCoor = nY   'Form Y coordinate of pixel in bitmap.
                nX = 0

                If nBitmapLocked = 0 Then           'Check if the bitmap area is already locked.  If not then proceed.  An exception will be raised
                    'if we did not release previously locked region.
                    nBitmapLocked = 1
                    mrecMainBMPArea.Y = nYCoor      'Note, the width, and x starting position is already declared when the application loaded.  We only need to
                    'set the y starting position.
                    mbitmapdataBMPArea = mbmpMainBMP.LockBits(mrecMainBMPArea, ImageLockMode.WriteOnly, mbmpMainBMP.PixelFormat)  'Assign a temporary buffer corresponds to the main bitmap.
                    'This is a 1D array to store 2D bitmap data, with each row of the bitmap being join to each other.  Thus in order to gain access to a pixel, we need to know:
                    ' a) It's (x,y) coordinate.
                    ' b) the Stride.  The Stride = (no. of pixels per row) x (no. of bytes per pixel).
                    ' Thus if we use Format24bpprgb format for the pixel, there are 3 bytes per pixel.
                    ' For a bitmap image of width = 160 pixels, the Stride = 480 bytes.
                End If
                nPixelSize = 3                          '3 bytes per pixel
                ptrBitmap = mbitmapdataBMPArea.Scan0    'Get the start address of bitmap memory.
                bytes = Math.Abs(mbitmapdataBMPArea.Stride) 'No. of bytes for 1 line of bitmap data.
                Dim rgbValues(bytes - 1) As Byte        ' Declare an array to hold the bytes of the bitmap.
                Runtime.InteropServices.Marshal.Copy(ptrBitmap, rgbValues, 0, bytes)  ' Copy the RGB values into the array.

                'Retrive gray scale data and plot on bitmap.  
                For nIndex = 0 To mintDataPayload1Length - 1
                    nData = mbytRxData1(nIndex)    'Get pixel luminance data or repetition number from the buffer.

                    If (nData > 128) Then               'Check if bit7 is set, this indicate repetition number.
                        nRepetition = nData And &H3F    'Mask out bit7 and bit6.
                        If (nRepetition > 0) Then       'Make sure repetition is not 0.
                            For nIndex2 = 0 To nRepetition - 1

                                nX = nX + 1
                                If (nX > mintImageWidth) Then 'Make sure X position is smaller than image width.
                                    Return
                                End If

                                nXCoor = nX        'Form X coordinate of pixel in bitmap.
                                nPixelIndex = nXCoor * nPixelSize

                                'Optional - store the pixel for subsequent image processing
                                'mbytPixel2(nX - 1, nY - 1) = nR8 >> 1 'Actual gray scale is only 7 bits, so we increase it's effective value!
                                mbytPixel2(nX - 1, nY - 1) = nR8

                                'Update pixel value in buffer. 
                                If mbytPixel(nX - 1, nY - 1) > 0 Then  'Check if it is to update the image pixel or marker/ROI pixel.

                                    Select Case mbytPixel(nX - 1, nY - 1)
                                        Case 1      'Red
                                            nColormR = 255
                                            nColormG = 0
                                            nColormB = 0
                                        Case 2      'Green
                                            nColormR = 0
                                            nColormG = 255
                                            nColormB = 0
                                        Case 3      'Blue
                                            nColormR = 0
                                            nColormG = 0
                                            nColormB = 255
                                        Case 4      'Yellow
                                            nColormR = 255
                                            nColormG = 255
                                            nColormB = 0
                                        Case 5      'Orange
                                            nColormR = 255
                                            nColormG = 165
                                            nColormB = 0
                                        Case Else   'Pink         
                                            nColormR = 255
                                            nColormG = 100
                                            nColormB = 100
                                    End Select
                                    'Update marker/ROI.
                                    rgbValues(nPixelIndex) = nColormB          'Blue component.
                                    rgbValues(nPixelIndex + 1) = nColormG      'Green component.
                                    rgbValues(nPixelIndex + 2) = nColormR      'Red component.

                                Else
                                    'Update image.  Note: the RGB components values are already set from the initial pixel
                                    'in the similar color group.
                                    rgbValues(nPixelIndex) = nColorB           'Blue component.
                                    rgbValues(nPixelIndex + 1) = nColorG       'Green component.
                                    rgbValues(nPixelIndex + 2) = nColorR       'Red component.
                                End If

                            Next nIndex2 'For nIndex2 = 0 To nRepetition - 1
                        End If 'If (nRepetition > 0)
                    Else            'Not repetition, new pixel data.
                        nX = nX + 1
                        If (nX > mintImageWidth) Then 'Make sure X position is smaller than image width.
                            Return
                        End If

                        nXCoor = nX                     'Form X coordinate of pixel in bitmap.
                        nPixelIndex = nXCoor * nPixelSize

                        nR8 = nData                     'Read pixel data
                        If nR8 > 127 Then               'Limit the maximum value.
                            nR8 = 127
                        End If

                        'Optional - store the pixel for subsequent image processing
                        'mbytPixel2(nX - 1, nY - 1) = nR8 >> 1 'Actual gray scale is only 7 bits, so we increase it's effective value!
                        mbytPixel2(nX - 1, nY - 1) = nR8

                        'Set the image pixel color scheme according to the luminance option selected.
                        If mintOption = mnLuminanceR Then
                            nColorR = 2 * nR8
                            nColorG = 0
                            nColorB = 0
                        ElseIf mintOption = mnLuminanceG Then
                            nColorR = 0
                            nColorG = 2 * nR8
                            nColorB = 0
                        ElseIf mintOption = mnLuminanceB Then
                            nColorR = 0
                            nColorG = 0
                            nColorB = 2 * nR8
                        ElseIf mintOption = mnHue Then
                            nHue = 4 * nR8  'Rescale back the hue value to between 0-360 for valid hue.
                            'In the remote unit the hue valus is divide by 4 before
                            'being transmitted.

                            'Algorithm to convert Hue to RGB color representation.
                            'Here valid value of hue is 0-360.
                            'nHue = 400 if no hue due to too dark.
                            'nHue = 420 if no hue due to too bright or near gray scale condition.
                            If nHue < 60 Then
                                'colorPixel = Color.FromArgb(255, (nHue / 60) * 255, 0)
                                nColorR = 255
                                nColorG = (nHue / 60) * 255
                                nColorB = 0
                            ElseIf nHue < 120 Then
                                'colorPixel = Color.FromArgb(255 * (1 - ((nHue - 60) / 60)), 255, 0)
                                nColorR = 255 * (1 - ((nHue - 60) / 60))
                                nColorG = 255
                                nColorB = 0
                            ElseIf nHue < 180 Then
                                'colorPixel = Color.FromArgb(0, 255, ((nHue - 120) / 60) * 255)
                                nColorR = 0
                                nColorG = 255
                                nColorB = ((nHue - 120) / 60) * 255
                            ElseIf nHue < 240 Then
                                'colorPixel = Color.FromArgb(0, 255 * (1 - ((nHue - 180) / 60)), 255)
                                nColorR = 0
                                nColorG = 255 * (1 - ((nHue - 180) / 60))
                                nColorB = 255
                            ElseIf nHue < 300 Then
                                'colorPixel = Color.FromArgb(((nHue - 240) / 60) * 255, 0, 255)
                                nColorR = ((nHue - 240) / 60) * 255
                                nColorG = 0
                                nColorB = 255
                            ElseIf nHue < 361 Then
                                'colorPixel = Color.FromArgb(255, 0, 255 * (1 - ((nHue - 300) / 60)))
                                nColorR = 255
                                nColorG = 0
                                nColorB = 255 * (1 - ((nHue - 300) / 60))
                            ElseIf nHue < 404 Then                'Saturation level too low, no color.
                                'colorPixel = Color.FromArgb(0, 0, 0)    'Black
                                nColorR = 0
                                nColorG = 0
                                nColorB = 0
                            Else
                                'colorPixel = Color.FromArgb(255, 255, 255)  'White
                                nColorR = 255
                                nColorG = 255
                                nColorB = 255
                            End If
                        Else                    'Either luminance or others.
                            nColorR = 2 * nR8   'Since the value is limited to 127, we multiply by 2 to increase the effective brightness.
                            nColorG = nColorR   'Convert to grayscale.
                            nColorB = nColorR
                        End If

                        'Form a pixel 
                        If mbytPixel(nX - 1, nY - 1) > 0 Then  'Check if it is to update the image pixel or marker/ROI pixel.

                            Select Case mbytPixel(nX - 1, nY - 1)   'Update marker/ROI pixel.
                                Case 1      'Red
                                    nColormR = 255
                                    nColormG = 0
                                    nColormB = 0
                                Case 2      'Green
                                    nColormR = 0
                                    nColormG = 255
                                    nColormB = 0
                                Case 3      'Blue
                                    nColormR = 0
                                    nColormG = 0
                                    nColormB = 255
                                Case 4      'Yellow
                                    nColormR = 255
                                    nColormG = 255
                                    nColormB = 0
                                Case 5      'Orange
                                    nColormR = 255
                                    nColormG = 165
                                    nColormB = 0
                                Case Else   'Pink         
                                    nColormR = 255
                                    nColormG = 100
                                    nColormB = 100
                            End Select
                            'Update marker/ROI.
                            rgbValues(nPixelIndex) = nColormB          'Blue component.
                            rgbValues(nPixelIndex + 1) = nColormG      'Green component.
                            rgbValues(nPixelIndex + 2) = nColormR      'Red component.

                        Else
                            'Update image.
                            rgbValues(nPixelIndex) = nColorB           'Blue component.
                            rgbValues(nPixelIndex + 1) = nColorG       'Green component.
                            rgbValues(nPixelIndex + 2) = nColorR       'Red component.
                        End If

                    End If 'If (nData > 128)

                Next nIndex

                'If nBitmapLocked = 1 Then  'If there is bitmap region locked, release it.
                Runtime.InteropServices.Marshal.Copy(rgbValues, 0, ptrBitmap, bytes)  ' Copy the RGB values back to the bitmap
                mbmpMainBMP.UnlockBits(mbitmapdataBMPArea)  'Copy the buffer data back to the bitmap.
                nBitmapLocked = 0
                'End If

                If (nYCoor Mod 2) = 0 Then  'Only update the bitmap display every other lines.
                    'graphicObject = Me.CreateGraphics

                    Dim srcRec = New Rectangle(1, nYCoor - 2, mbmpMainBMP.Width, 2)
                    'graphicObject.DrawImage(mbmpMainBMP, mptBmpStart)                  'Display whole bitmap 
                    'graphicObject.DrawImage(mbmpMainBMP, mptBmpStart.X, mptBmpStart.Y + 2 * nYCoor, srcRec, GraphicsUnit.Pixel) 'Display only 2 rows

                    'mgraphicObject.DrawImage(mbmpMainBMP, mptBmpStart)                  'Display whole bitmap 
                    mgraphicObject.DrawImage(mbmpMainBMP, mptBmpStart.X, mptBmpStart.Y + 2 * nYCoor, srcRec, GraphicsUnit.Pixel) 'Display only 2 rows

                    'graphicObject.Dispose()
                    'Release all resources used by graphic object.  Note: 11 July 2019, if this is not done,
                    'I observed that the memory used by this application will slowly increase over time while
                    'debugging the software in Visual Studio.  Alternative is to use global graphic resources, 
                    'so that we do not have to create new graphic resources every time.
                End If
            End If 'End of If (nY <= mintImageHeight) Then

            mintCurrentBMPLine1Counter = 100000         'Invalidate current line so that this function will not run anymore until a new line data has been buffered.

        Catch ex As ThreadAbortException

        Catch ex As Exception
            MessageBox.Show(ex.Message, "DisplayBitmap Error at line " & CStr(nY), MessageBoxButtons.OK)
        End Try
    End Sub

    'Last updated:  14 Aug 2018
    'Author:        Fabian Kung
    'Subroutine to update the bitmap image one line at a time.  Also superimpose markers or ROI.
    'No compression is assumed.
    '
    '--- 1. Format for 1 line of pixel data ---
    'The data format:
    'Byte0: 0xFF (Indicate start-of-line)
    'Byte1: Line number, 0-253, indicate the line number In the bitmap image.
    '       if Byte1 = 254, it indicate the following bytes are secondary data.  
    'Byte2: The length Of the data payload, excluding Byte0-2.
    'Byte3-ByteN: Data payload.
    'The data payload only accepts 8 bits value, from bit0-bit7.  

    'Example
    'Consider the following byte stream:
    '[0xFF][3][7][90][80][70][0x83][60][0x82][120]
    'Byte2 = 7 indicates that there are 7 data bytes in this data packet.
    'Byte3 = 90 represent the 1st pixel data in line 3.  This value can represent the 
    '         luminance value of other user define data.
    'Byte4 = 80 another pixel data.
    'Byte5 = 70 another pixel data.
    'Byte6 = 0x83 another pixel data.
    ' ....
    'Byte9 = 120, another pixel.
    '
    ' --- 2. Markers/ROI ---
    ' We can ovellap marker or ROI (region of interest) onto the bitmap image.
    ' A 2D array mbytPixel(x,y) is used to keep track of the shape and size of markers or ROI that 
    ' would be superimposed onto the bitmap image.  
    ' Whenever the value of the element in mbytPixel(x,y) is <> 0, the pixel in the bitmap will 
    ' be colored according to the value in mbytPixel(x,y).  If it is 0, the color of the bitmap
    ' will be determined by the image pixel data received from the external module.
    Private Sub DisplayBitmapNoCompression()

        Static Dim nR8 As Integer
        Dim nData As Integer
        Dim nX As Integer
        Dim nY As Integer
        Dim nIndex As Integer
        Dim nXCoor As Integer
        Dim nYCoor As Integer
        Dim graphicObject As Graphics
        Dim ptrBitmap As IntPtr
        Dim nPixelSize As Integer
        Dim nPixelIndex As Integer
        Dim bytes As Integer
        Static Dim nBitmapLocked As Integer = 0
        Static Dim nColorR As Integer       'RGB components for pixel color.
        Static Dim nColorG As Integer
        Static Dim nColorB As Integer
        Static Dim nColormR As Integer      'RGB components for marker color.
        Static Dim nColormG As Integer
        Static Dim nColormB As Integer

        Try

            nY = mintCurrentBMPLine1Counter + 1  'Get line number, add 1 since the value starts from 0.  The bitmap coordinate index starts from 1

            If (nY <= mintImageHeight) Then         'Make sure line no. is less than image height in pixels.

                'window coordinate starts from 1.
                nYCoor = nY   'Form Y coordinate of pixel in bitmap.
                nX = 0

                If nBitmapLocked = 0 Then           'Check if the bitmap area is already locked.  If not then proceed.  An exception will be raised
                    'if we did not release previously locked region.
                    nBitmapLocked = 1
                    mrecMainBMPArea.Y = nYCoor      'Note, the width, and x starting position is already declared when the application loaded.  We only need to
                    'set the y starting position.
                    mbitmapdataBMPArea = mbmpMainBMP.LockBits(mrecMainBMPArea, ImageLockMode.WriteOnly, mbmpMainBMP.PixelFormat)  'Assign a temporary buffer corresponds to the main bitmap.
                    'This is a 1D array to store 2D bitmap data, with each row of the bitmap being join to each other.  Thus in order to gain access to a pixel, we need to know:
                    ' a) It's (x,y) coordinate.
                    ' b) the Stride.  The Stride = (no. of pixels per row) x (no. of bytes per pixel).
                    ' Thus if we use Format24bpprgb format for the pixel, there are 3 bytes per pixel.
                    ' For a bitmap image of width = 160 pixels, the Stride = 480 bytes.
                End If
                nPixelSize = 3                          '3 bytes per pixel
                ptrBitmap = mbitmapdataBMPArea.Scan0    'Get the start address of bitmap memory.
                'bytes = Math.Abs(mbitmapdataBMPArea.Stride) * mbmpMainBMP.Height
                bytes = Math.Abs(mbitmapdataBMPArea.Stride)
                Dim rgbValues(bytes - 1) As Byte        ' Declare an array to hold the bytes of the bitmap.
                Runtime.InteropServices.Marshal.Copy(ptrBitmap, rgbValues, 0, bytes)  ' Copy the RGB values into the array.

                'Retrive gray scale data and plot on bitmap.  
                For nIndex = 0 To mintDataPayload1Length - 1
                    nData = mbytRxData1(nIndex)    'Get pixel luminance data or repetition number from the buffer.

                    nX = nX + 1
                    If (nX > mintImageWidth) Then 'Make sure X position is smaller than image width.
                        Return
                    End If

                    nXCoor = nX                     'Form X coordinate of pixel in bitmap.
                    nPixelIndex = nXCoor * nPixelSize

                    nR8 = nData                     'Read pixel data
                    If nR8 > 255 Then               'Limit the maximum value.
                        nR8 = 255
                    End If

                    nColorR = nR8
                    nColorG = nColorR   'Convert to grayscale.
                    nColorB = nColorR

                    'Form a pixel 
                    If mbytPixel(nX - 1, nY - 1) > 0 Then  'Check if it is to update the image pixel or marker/ROI pixel.

                        Select Case mbytPixel(nX - 1, nY - 1)   'Update marker/ROI pixel.
                            Case 1      'Red
                                nColormR = 255
                                nColormG = 0
                                nColormB = 0
                            Case 2      'Green
                                nColormR = 0
                                nColormG = 255
                                nColormB = 0
                            Case 3      'Blue
                                nColormR = 0
                                nColormG = 0
                                nColormB = 255
                            Case 4      'Yellow
                                nColormR = 255
                                nColormG = 255
                                nColormB = 0
                            Case 5      'Orange
                                nColormR = 255
                                nColormG = 165
                                nColormB = 0
                            Case Else   'Pink         
                                nColormR = 255
                                nColormG = 100
                                nColormB = 100
                        End Select
                        'Update marker/ROI.
                        rgbValues(nPixelIndex) = nColormB          'Blue component.
                        rgbValues(nPixelIndex + 1) = nColormG      'Green component.
                        rgbValues(nPixelIndex + 2) = nColormR      'Red component.

                    Else
                        'Update image.
                        rgbValues(nPixelIndex) = nColorB           'Blue component.
                        rgbValues(nPixelIndex + 1) = nColorG       'Green component.
                        rgbValues(nPixelIndex + 2) = nColorR       'Red component.
                    End If
                Next nIndex

                'If nBitmapLocked = 1 Then  'If there is bitmap region locked, release it.
                Runtime.InteropServices.Marshal.Copy(rgbValues, 0, ptrBitmap, bytes)  ' Copy the RGB values back to the bitmap
                mbmpMainBMP.UnlockBits(mbitmapdataBMPArea)  'Copy the buffer data back to the bitmap.
                nBitmapLocked = 0
                'End If

                If (nYCoor Mod 2) = 0 Then  'Only update the bitmap display every other lines.
                    graphicObject = Me.CreateGraphics

                    Dim srcRec = New Rectangle(1, nYCoor - 2, mbmpMainBMP.Width, 2)
                    'graphicObject.DrawImage(mbmpMainBMP, mptBmpStart)                  'Display whole bitmap 
                    graphicObject.DrawImage(mbmpMainBMP, mptBmpStart.X, mptBmpStart.Y + 2 * nYCoor, srcRec, GraphicsUnit.Pixel) 'Display only 2 rows

                    graphicObject.Dispose()
                    'Release all resources used by graphic object.  Note: 30 April 2018, if this is not done,
                    'I observed that the memory used by this application will slowly increase over time while
                    'debugging the software in Visual Studio.  
                End If
            End If 'End of If (nY <= mintImageHeight) Then

            mintCurrentBMPLine1Counter = 100000         'Invalidate current line so that this function will not run anymore until a new line data has been buffered.

        Catch ex As ThreadAbortException

        Catch ex As Exception
            MessageBox.Show(ex.Message, "DisplayBitmapNoCompression Error at line " & CStr(nY), MessageBoxButtons.OK)
        End Try
    End Sub


    'Date:      16 September 2019
    'Author:    Fabian Kung
    'Subroutine to draw rectangles on the secondary display buffer.  The rectangles function as markers
    'This version supports up to two rectangles.  
    'Beside the rectangle, there is also a byte of debugging data that can be tag along.  The content
    'of the debugging byte, mnDataDebug is decided by the MVM firmware.
    '
    'Mechanism:
    'The software maintains a secondary display buffer in the form of a 2D array mbytPixel. 
    'Each element of mbytPixel corresponds to the pixel
    'in the video display window.  The value stored in mbytPixel indicates whether the corresponding pixel 
    'in the video display window should be highlight or not.  If the value stored is 0, the pixel is not 
    'highlighted, i.e. the pixel color is determined by the RGB value of the video stream.
    'Otherwise the pixel will be highlighted, with the color of the pixel corresponds to the value
    'specified in nColorSetting below.
    Private Sub DisplaySecondaryInfo()

        Dim nXindex As Integer
        Dim nYindex As Integer
        Dim nMIndex As Integer

        'Rectangles detail
        Dim nXstart(2) As Integer
        Dim nYstart(2) As Integer
        Dim nXsize(2) As Integer
        Dim nYsize(2) As Integer
        Static Dim nXstartold(2) As Integer
        Static Dim nYstartold(2) As Integer
        Static Dim nXsizeold(2) As Integer
        Static Dim nYsizeold(2) As Integer
        Dim nColorSetting(2) As Integer
        'Dim nXindexStart As Integer
        Dim nXindexEnd As Integer
        'Dim nYindexStart As Integer
        Dim nYindexEnd As Integer

        Try

            nXsize(0) = mbytRxData1(0)         'Get the location and size of marker 1 if present.    
            nYsize(0) = mbytRxData1(1)
            nXstart(0) = mbytRxData1(2)
            nYstart(0) = mbytRxData1(3)
            nColorSetting(0) = mbytRxData1(4)  'Get color of marker 1.
            nXsize(1) = mbytRxData1(5)         'Get the location and size of marker 2 if present.    
            nYsize(1) = mbytRxData1(6)
            nXstart(1) = mbytRxData1(7)
            nYstart(1) = mbytRxData1(8)
            nColorSetting(1) = mbytRxData1(9)  'Get color of marker 2.
            mnDataDebug = mbytRxData1(10)      'Get debug data.
            mnDataIPA1 = mbytRxData1(11)
            mnDataIPA2 = mbytRxData1(12)
            'mnDataDebug = mnDebug


            'Debug, send info to Main Window
            mnData1 = nXstart(0)
            mnData2 = nYstart(0)
            mnData3 = nXstart(1)
            mnData4 = nYstart(1)

            'Highlight rectangular marker 1 in the Secondary Display Buffer.
            'We do this by scanning through each pixel in the image frame.  For those pixel which is to be changed to
            'marker color set the correponding value in mbytPixel(x,y) to a non-zero value, the value is the marker color.
            'if mbytPixel(x,y) value is 0, the corresponding pixel takes the value of the image frame.
            For nMIndex = 0 To 1
                If (nXstart(nMIndex) < (mintImageWidth - 1)) And (nYstart(nMIndex) < (mintImageHeight - 1)) Then

                    For nYindex = nYstartold(nMIndex) To (nYstartold(nMIndex) + nYsizeold(nMIndex))     'Clear previous marker/ROI
                        For nXindex = nXstartold(nMIndex) To (nXstartold(nMIndex) + nXsizeold(nMIndex))
                            If (nYindex <> mintYCrossHair) And (nXindex <> mintXCrossHair) Then 'Make sure not on the cross-hair
                                mbytPixel(nXindex, nYindex) = 0
                            End If
                        Next
                    Next

                    nXindexEnd = nXstart(nMIndex) + nXsize(nMIndex)     'Limit the extend of the marker along x coordinate.    
                    If (nXindexEnd > (mintImageWidth - 1)) Then
                        nXindexEnd = mintImageWidth - 1
                    End If
                    nYindexEnd = nYstart(nMIndex) + nYsize(nMIndex)     'Limit the extend of the marker along y coordinate.    
                    If (nYindexEnd > (mintImageHeight - 1)) Then
                        nYindexEnd = mintImageHeight - 1
                    End If

                    'Check if bit 7 of the color setting is set.  If set then only the outline of the ROI
                    'is drawn, else fill the ROI.
                    If (nColorSetting(nMIndex) > 127) Then 'Bit 7 = 128 in decimal
                        'Draw the 4 borders on the square region.
                        nColorSetting(nMIndex) = nColorSetting(nMIndex) - 128 'Clear bit7 to extract the color
                        nYindex = nYstart(nMIndex)
                        If nYindex <> mintYCrossHair Then      'Make sure not on cross-hair.
                            For nXindex = nXstart(nMIndex) To nXindexEnd  'Horizontal border 1.
                                mbytPixel(nXindex, nYindex) = nColorSetting(nMIndex)
                            Next
                        End If
                        nYindex = nYindexEnd
                        If nYindex <> mintYCrossHair Then      'Make sure not on cross-hair.
                            For nXindex = nXstart(nMIndex) To nXindexEnd  'Horizontal border 2.
                                mbytPixel(nXindex, nYindex) = nColorSetting(nMIndex)
                            Next
                        End If
                        nXindex = nXstart(nMIndex)
                        If nXindex <> mintXCrossHair Then      'Make sure not on cross-hair.
                            For nYindex = nYstart(nMIndex) To nYindexEnd  'Vertical border 1.
                                mbytPixel(nXindex, nYindex) = nColorSetting(nMIndex)
                            Next
                        End If

                        nXindex = nXindexEnd
                        If nXindex <> mintXCrossHair Then      'Make sure not on cross-hair.
                            For nYindex = nYstart(nMIndex) To nYindexEnd  'Vertical border 2.
                                mbytPixel(nXindex, nYindex) = nColorSetting(nMIndex)
                            Next
                        End If

                    Else                                                     'Fill a rectangular region.   
                        For nYindex = nYstart(nMIndex) To nYindexEnd     'Highlight current marker/ROI
                            For nXindex = nXstart(nMIndex) To nXindexEnd
                                If (nYindex <> mintYCrossHair) And (nXindex <> mintXCrossHair) Then 'Make sure not on the cross-hair
                                    mbytPixel(nXindex, nYindex) = nColorSetting(nMIndex)
                                End If
                            Next
                        Next
                    End If

                    nXstartold(nMIndex) = nXstart(nMIndex)                'Store current parameters.
                    nYstartold(nMIndex) = nYstart(nMIndex)
                    nXsizeold(nMIndex) = nXindexEnd - nXstart(nMIndex)
                    nYsizeold(nMIndex) = nYindexEnd - nYstart(nMIndex)
                End If
            Next
        Catch ex As ThreadAbortException

        Catch ex As Exception
            MessageBox.Show(ex.Message, "DisplaySecondaryInfo Error ", MessageBoxButtons.OK)
        End Try
    End Sub


    'Thread 1: RobotEyeMon Receive Buffer
    'This thread uses a state machine to read one line of pixel data into the display buffer.
    '
    'Each packet consists of:
    'Byte0: 0xFF (Start-of-line code)
    'Byte1: Line number for bitmap
    'Byte2: No. of bytes in the payload
    'Byte3 to last byte: payload.
    'For instance if Byte2 = 50, then last byte will be Byte53.
    '
    'The state machine reads the first 3 bytes, then decides how many bytes to 
    'anticipate for the payload.  After reading the payload, it will send an
    'instruction back to the remote device to send the next line of pixel data.
    'The payload data is compressed using simple RLE (Run-Length Encoding)
    'method. See further details on the DisplayBitmap() subroutine.
    '
    'There are two dispay buffers, mbytRxData1[] and mbytRxData2[].  This is to improve
    'the efficiency and prevent conflict for the buffering and displaying process.  
    'Which buffers will be used by this thread is determined by the mutex 
    'For instance if this thread is buffering pixel data into mbytRxData1[], 
    'the DisplayBitmap() subroutine will access the pixel data in mbytRxData2[] and 
    'update the picturebox.  Which display buffer will be used by Thread1 and
    'DisplayBitmap() is determined by the mutexes mblnLine1Mutex and mblnLine2Mutex.
    '
    'For instance if mblnLine1Mutex = True and mblnLine2Mutex = False, this indicates
    'DisplayBitmap() subroutine is updating the picturebox using the pixel data stored
    'in mbytRxData2[], so buffering of data from COM port should be to mbytRxData1[].

    Public Sub ThreadProc1()
        Static Dim nState As Integer = 0
        Static Dim nLine As Integer
        Static Dim nLength As Integer
        Dim nRxData As Integer

        Try
            While 1
                If SerialPort1.IsOpen = True Then
                    Select Case nState
                        Case 0 'Line 1: Check for start-of-line code and get the line no. and packet length.
                            If mintCurrentBMPLine1Counter < mintImageHeight Then 'Make sure pixel line is within display area.
                                If mintOption <> mnIPResult Then
                                    DisplayBitmap()                 'Use display bitmap routine with line compression.
                                Else
                                    DisplayBitmapNoCompression()    'Use display bitmap routine without line compression.
                                End If
                            ElseIf mintCurrentBMPLine1Counter = 254 Then        'If equal 254 then proceed to show secondary information.
                                DisplaySecondaryInfo()
                            End If

                            mnDebug = 0
                            If (SerialPort1.BytesToRead > 2) Then 'Start-of-line code, line number and packet length, 3 bytes.
                                nRxData = SerialPort1.ReadByte()
                                If (nRxData = &HFF) Then                'Start-of-line code?
                                    nLine = SerialPort1.ReadByte()      'Yes, proceed to get image line no. and
                                    nLength = SerialPort1.ReadByte()    'payload lenght in bytes.
                                    nState = 1
                                Else
                                    SerialPort1.DiscardInBuffer() 'Flush buffer, wrong format
                                    nState = 0
                                End If
                            Else
                                nState = 0
                            End If

                        Case 1 'Line 1: Get the rest of the payload.
                            mnDebug = 1
                            If SerialPort1.BytesToRead > (nLength - 1) Then   'Wait until full line of pixel data has arrived
                                'If mblnLine1Mutex = True Then                               'Make sure the receive buffer1 is not in used
                                mintCurrentBMPLine1Counter = nLine
                                    mintDataPayload1Length = nLength
                                    SerialPort1.Read(mbytRxData1, 0, mintDataPayload1Length) 'Read single line pixel data into buffer1
                                    SerialPort1.DiscardInBuffer()                           'Flush buffer to prepare for incoming data.

                                    'Ask the remote client to send another line of pixel data.
                                    Select Case mintOption  'Check option and send the correct request.
                                        Case 0
                                            mbytTxData(0) = Asc("L")    'Ask remote unit to send luminance data of pixel
                                        Case 1
                                            mbytTxData(0) = Asc("R")   'Ask remote unit to send luminance data of pixel (R only)
                                        Case 2
                                            mbytTxData(0) = Asc("G")   'Ask remote unit to send luminance data of pixel (G only)
                                        Case 3
                                            mbytTxData(0) = Asc("B")   'Ask remote unit to send luminance data of pixel (B only)
                                        Case 4
                                            mbytTxData(0) = Asc("D")   'Ask remote unit to send luminance gradient data of pixel
                                        Case 5
                                            mbytTxData(0) = Asc("H")   'Ask remote unit to send hue data of pixel
                                        Case Else
                                            mbytTxData(0) = Asc("P")   'Ask remote unit to send the result of image processing algorithm on pixel data
                                    End Select

                                    mbytTxData(1) = 0
                                    SerialPort1.Write(mbytTxData, 0, 1) 'Send a command byte to remote client to start the process
                                    nState = 0

                            Else
                                nState = 1
                                'Thread.Sleep(0)
                            End If

                        Case Else
                            nState = 0
                            'Thread.Sleep(0)
                    End Select
                End If 'If SerialPort1.IsOpen = True
            End While

            'Catch ex As Exception
        Catch ex As ThreadAbortException
            'MessageBox.Show("Thread1: " & CStr(nState) & ": " & ex.Message, "ERROR", MessageBoxButtons.OK)
        End Try

    End Sub



    Private Sub ButtonHLUp_Click(sender As Object, e As EventArgs) Handles ButtonHLUp.Click

        Dim nIndex As Integer

        If mintYCrossHair > 1 Then
            mintYCrossHair = mintYCrossHair - 1
            For nIndex = 0 To mintImageWidth - 1
                If nIndex <> mintXCrossHair Then
                    mbytPixel(nIndex, mintYCrossHair + 1) = 0      'Clear the current pixels of marker.
                    mbytPixel(nIndex, mintYCrossHair) = mnColor    'Mark the next row of pixels.
                End If
            Next

        End If

    End Sub


    Private Sub ButtonHLDown_Click(sender As Object, e As EventArgs) Handles ButtonHLDown.Click

        Dim nIndex As Integer

        If mintYCrossHair < mintImageHeight - 1 Then
            mintYCrossHair = mintYCrossHair + 1
            For nIndex = 0 To mintImageWidth - 1
                If nIndex <> mintXCrossHair Then
                    mbytPixel(nIndex, mintYCrossHair - 1) = 0      'Clear the current pixels of marker.
                    mbytPixel(nIndex, mintYCrossHair) = mnColor    'Mark the next row of pixels.
                End If
            Next
        End If

    End Sub

    Private Sub ButtonVLRight_Click(sender As Object, e As EventArgs) Handles ButtonVLRight.Click
        Dim nIndex As Integer

        If mintXCrossHair < mintImageHeight - 1 Then
            mintXCrossHair = mintXCrossHair + 1
            For nIndex = 0 To mintImageHeight - 1
                If nIndex <> mintYCrossHair Then
                    mbytPixel(mintXCrossHair - 1, nIndex) = 0      'Clear the current pixels of marker.
                    mbytPixel(mintXCrossHair, nIndex) = mnColor    'Mark the next column of pixels.
                End If
            Next

        End If
    End Sub

    Private Sub ButtonVLLeft_Click(sender As Object, e As EventArgs) Handles ButtonVLLeft.Click
        Dim nIndex As Integer

        If mintXCrossHair > 1 Then
            mintXCrossHair = mintXCrossHair - 1
            For nIndex = 0 To mintImageHeight - 1
                If nIndex <> mintYCrossHair Then
                    mbytPixel(mintXCrossHair + 1, nIndex) = 0      'Clear the current pixels of marker.
                    mbytPixel(mintXCrossHair, nIndex) = mnColor    'Mark the next column of pixels.
                End If
            Next

        End If
    End Sub

    Private Sub ButtonSaveImage_Click(sender As Object, e As EventArgs) Handles ButtonSaveImage.Click
        Dim nYindex As Integer
        Dim nXindex As Integer
        Dim strString As String

        If mFilePath = "" Then
            OpenFileDialog1.Title = "Select or create a raw image file"
            OpenFileDialog1.CheckFileExists = False                 'Don't check if filename exists.
            OpenFileDialog1.InitialDirectory = CurDir()             'Use current system directory or path.
            If OpenFileDialog1.ShowDialog() = DialogResult.OK Then  'If the specified filename is normal, open the file.
                mFilePath = OpenFileDialog1.FileName
            End If
        End If

        'Note: The image frame is stored as an array of 2D pixels, each with value from 0-255.
        'The x index corresponds to the row, and y index corresponds to the height.
        'Here we write the pixel data to the file row-by-row.
        If mFilePath <> "" Then
            Using sw As StreamWriter = File.CreateText(mFilePath)
                For nYindex = 1 To mintImageHeight - 1
                    strString = ""                  'Clear the string first.
                    For nXindex = 1 To mintImageWidth - 1
                        strString = strString & ChrW(mbytPixel2(nXindex, nYindex)) 'Form a line representing a line of pixel data.  Somehow 
                        'the binary value in mbytPixel() array is store as it
                        'is into the strString array with ChrW() function is used.
                    Next
                    sw.WriteLine(strString)     'Write the pixel data to file, a NL/CR character will be appended at the end of each line.
                Next
                LabelInfo.Text = "Save complete"
            End Using
        End If

        '---Alternative approach using WriteAllBytes() method---
        'Dim bytData(0 To 160) As Byte

        '-Write 1st line to file-
        'nYindex = 1
        'For nXindex = 1 To mintImageWidth - 1
        '  bytData(nXindex) = mbytPixel2(nXindex, nYindex)
        'Next
        'bytData(160) = 255  'A value to indicate end-of-line (EOL).
        'My.Computer.FileSystem.WriteAllBytes(mFilePath, bytData, False)

        'For nYindex = 2 To mintImageHeight - 1
        '  For nXindex = 1 To mintImageWidth - 1
        '    bytData(nXindex) = mbytPixel2(nXindex, nYindex)
        '  Next
        '  bytData(160) = 255  'A value to indicate end-of-line (EOL).
        '  My.Computer.FileSystem.WriteAllBytes(mFilePath, bytData, True)
        'Next

    End Sub

End Class

