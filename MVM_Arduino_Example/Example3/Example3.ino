// Example 3
// Hardware: Arduino Uno Rev 3 or equivalent.
// 2x RC servo motors.
// MVM V1.5C.
// Connections - Pin 4 to azimuth RC servo motor.
//               Pin 7 to elevation RC servo motor.
//               Pin 10 to MVM V1.5C TX pin
//               Pin 11 to MVM V1.5C RX pin (via 100 to 330 Ohm series resistor).
// Description:
// In this example the Arduino will drive the RC motors such that the robot head will track a color 
// object.

#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

int nNoObjectTimer = 0;
int nHeadElevationAngle_Deg;
int nHeadAzimuthAngle_Deg;
#define   _PMOTOR_AZI  4
#define   _PMOTOR_ELE  7
//#define   _PMOTOR_AZI  3
//#define   _PMOTOR_ELE  12

void setup() {
  // put your setup code here, to run once:
  pinMode(2,OUTPUT);
  pinMode(_PMOTOR_AZI,OUTPUT);
  pinMode(_PMOTOR_ELE,OUTPUT);
  digitalWrite(_PMOTOR_AZI,LOW);
  digitalWrite(_PMOTOR_ELE,LOW);
  Serial.begin(57600);
  mySerial.begin(57600);
  delay(1000);          // A 1000 ms delay for the module to initialize properly.
  mySerial.write(0x30);   // Run IPA (image processing algorithm) 3 on interval 1. Yellow-green object.
  mySerial.write(0x30);   // Run IPA 3 on interval 2.
  //mySerial.write(0x31);   // Run IPA (image processing algorithm) 3 on interval 1. Red object.
  //mySerial.write(0x31);   // Run IPA 3 on interval 2.  
  nHeadAzimuthAngle_Deg = 90;  // Drive azimuth and elevation motors to initial position.
  nHeadElevationAngle_Deg = 90;
  mySerial.flush();     // Flush TX and RX buffers before we start.
} 

void loop() {
int nID, nPixelHit, nX, nY;
int nErrorX, nErrorY;


  if (mySerial.available()>3)     // One data packet consists of 4 bytes.
  {
    nID = mySerial.read();        // Get IPA ID.
    if (nID == 3)                 // Make sure it is algorithm 3.
    {
      digitalWrite(2,HIGH);         // Generate a pulse for probing purpose.
      nPixelHit = mySerial.read();  // Get number of interior pixels matching the color criteria.
      nX = mySerial.read();         // Get (x,y) coordinate in the image frame of the object
      nY = mySerial.read();         // matching the color criteria.
      
      //Serial.print("Process ID = ");
      //Serial.print(nID);
      //Serial.println();
      //Serial.print("X coordinate = ");
      //Serial.println(nX);
      //Serial.print("Y coordinate = ");
      //Serial.println(nY);
      
      //Serial.print("nErrorX = ");
      //Serial.println(nErrorX);
      Serial.print("Azimuth angle = ");
      Serial.println(nHeadAzimuthAngle_Deg);
      //Serial.print("nErrorY = ");
      //Serial.println(nErrorY);        
      Serial.print("Elevation angle = ");
      Serial.println(nHeadElevationAngle_Deg);      
      
      if (nX < 255)               // Only adjust the head if either x or y coordinate is < 255.
                                  // 255 is -1 in 8-bit signed two's complement format, which means
                                  // invalid coordinate, e.g. object not found.
      {
        nNoObjectTimer = 0;       // Reset no object detect timer.
        nErrorX = 80 - nX;        // Calculate the horizontal position (x axis)
                                  // from the center of field of view (FOV).  
        nErrorY = 60 - nY;        // Subtract the y coordinate from the center
                                  // of the FOV (field-of-view).   
        if (nErrorX > 0)          // Adjust azimuth motor angle.
        { 
          nErrorX = nErrorX >> 3; // Scale azimuth error magnitude, e.g. divide by 8.
          nHeadAzimuthAngle_Deg = nHeadAzimuthAngle_Deg - nErrorX; // Adjust azimuth angle setting.
        }
        else if (nErrorX < 0)
        {
          nErrorX = (-nErrorX) >> 3;  // Scale the error magnitude.
          nHeadAzimuthAngle_Deg = nHeadAzimuthAngle_Deg + nErrorX;  // Adjust azimuth angle setting.
        }
        if (nHeadAzimuthAngle_Deg < 0)  // Make sure angle setting is within 0 to 180 degrees.
        {
          nHeadAzimuthAngle_Deg = 0; 
        }
        else if (nHeadAzimuthAngle_Deg > 180)
        {
          nHeadAzimuthAngle_Deg = 180;
        }
        
        if (nErrorY > 0)          // Adjust elevation angle motor angle.
        { 
          nErrorY = nErrorY >> 3; // Scale elevation error magnitude, e.g. divide by 8.
          nHeadElevationAngle_Deg = nHeadElevationAngle_Deg + nErrorY;
        }
        else if (nErrorY < 0)
        {
          nErrorY = (-nErrorY) >> 3;  // Scale the error magnitude.
          nHeadElevationAngle_Deg = nHeadElevationAngle_Deg - nErrorY;  
        }
        if (nHeadElevationAngle_Deg < 0)  // Make sure angle setting is within 0 to 180 degrees.
        {
          nHeadElevationAngle_Deg = 0; 
        }
        else if (nHeadElevationAngle_Deg > 180)
        {
          nHeadElevationAngle_Deg = 180;
        }
             
      } // if (nX < 255)  
      else
      {
        
        nNoObjectTimer++;         // Increment no object timer, each increment corresponds to 50 msec duration.
        if (nNoObjectTimer > 40)  // If > 2 seconds no object detected, set motors to default position.
        {
          nNoObjectTimer = 0;     // Clear no object timer.
          nHeadAzimuthAngle_Deg = 90;  
          nHeadElevationAngle_Deg = 90;          
        } // if (nNoObjectTimer > 40)
      }
      digitalWrite(2,LOW);                                                                
    } // if (nID == 3)
    else
    {
      mySerial.flush();         // Wrong data packet. Discard.
    } // if (nID == 3)   

    DriveAzimuthMotor(nHeadAzimuthAngle_Deg);  // Drive azimuth motor.
    DriveElevationMotor(nHeadElevationAngle_Deg);  // Drive elevation motor.
  } // if (mySerial.available()>3)
}

void DriveAzimuthMotor(int nAngle)
{
  int nMotorPulseWidth;
  
  nMotorPulseWidth = map(nAngle, 0, 180, 900, 1800);
  digitalWrite(_PMOTOR_AZI,HIGH);
  delayMicroseconds(nMotorPulseWidth);
  digitalWrite(_PMOTOR_AZI,LOW);
}

void DriveElevationMotor(int nAngle)
{
  int nMotorPulseWidth;
  
  nMotorPulseWidth = map(nAngle, 0, 180, 900, 1800);
  digitalWrite(_PMOTOR_ELE,HIGH);
  delayMicroseconds(nMotorPulseWidth);
  digitalWrite(_PMOTOR_ELE,LOW);
}
