// Example 4
// Hardware: Arduino Uno Rev 3 or equivalent.
// 2x RC servo motors, L298N DC motor driver,
// 2x toy DC motor with wheels, 2S 7.4V Lipo 
// battery.
// MVM V1.5C.
// Connections - Pin 3 to azimuth RC servo motor.
//               Pin 12 to elevation RC servo motor.
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
#define   _PMOTOR_AZI  3
#define   _PMOTOR_ELE  12

// Movement modes
#define _FORWARD 1
#define _REVERSE 2
#define _LEFT    3
#define _RIGHT   4
#define _STOP    0
int nMoveMode = _STOP;
int nSpeed = 7;
int nTimer = 10;
int nState = 0;

unsigned int unCounter = 0;

// For the DC Motor Driver
int pMotorL1 = 2;
int pMotorL2 = 4;
int pMotorR1 = 7;
int pMotorR2 = 8;
int pMotorLSpeedCtrl = 5;  // These two pins has PWM capability.  Also
int pMotorRSpeedCtrl = 6;  // on the Arduino reference, pin 5 and 6 has
                           // similar PWM clock.
void setup() {
  // put your setup code here, to run once:
  pinMode(_PMOTOR_AZI,OUTPUT);
  pinMode(_PMOTOR_ELE,OUTPUT);
  digitalWrite(_PMOTOR_AZI,LOW);
  digitalWrite(_PMOTOR_ELE,LOW);
  Serial.begin(57600);
  mySerial.begin(57600);
  delay(1000);          // A 1000 ms delay for the module to initialize properly.
  mySerial.write(0x20);   // Run IPA (image processing algorithm) 2 on interval 1. Obstacle detection.
  mySerial.write(0x20);   // Run IPA 2 on interval 2.
  nHeadAzimuthAngle_Deg = 74;    // Drive azimuth and elevation motors to initial position.
  nHeadElevationAngle_Deg = 25;  // Look down, adjust both angles as you see fit.
  mySerial.flush();     // Flush TX and RX buffers before we start.
  delay(1000);
  mySerial.write(0x20);     // Not sure why sometimes the MVM does not receive the command, thus resend
  mySerial.write(0x20);     // again to be double sure, could be bad wiring or EMC issue.
} 

void loop() {
int nID; 
int nPixelHit, nX, nY;            // Variables to store data packet from IPA3.
int nRow0, nRow1, nRow2;          // Variables to store data packet from IPA2.
int nErrorX, nErrorY;


  if (mySerial.available()>3)     // One data packet consists of 4 bytes.
  {
    nID = mySerial.read();        // Get IPA ID.
    if (nID == 3)                 // Check if data packet is from IPA 3.
    {
      //digitalWrite(2,HIGH);         // Generate a pulse for probing purpose.
      nPixelHit = mySerial.read();  // Get number of interior pixels matching the color criteria.
      nX = mySerial.read();         // Get (x,y) coordinate in the image frame of the object
      nY = mySerial.read();         // matching the color criteria.
      
      Serial.print("Process ID = ");
      Serial.print(nID);
      Serial.println();
      Serial.print("X coordinate = ");
      Serial.println(nX);
      Serial.print("Y coordinate = ");
      Serial.println(nY);
                                                          
    } // if (nID == 3)
    else if (nID == 2)              // Check if data packet is from IPA 2.
    {
      nRow2 = mySerial.read();      // Get Row 2 status.
      nRow1 = mySerial.read();      // Get Row 1 status.
      nRow0 = mySerial.read();      // Get Row 0 status.
      
      Serial.print("Process ID = ");
      Serial.print(nID);
      Serial.println();
      Serial.print("Row 2 = ");
      Serial.println(nRow2);
      Serial.print("Row 1 = ");
      Serial.println(nRow1);      
    }
    else
    {
      mySerial.flush();         // Wrong data packet. Discard.
    } // if (nID == 3)   

    DriveAzimuthMotor(nHeadAzimuthAngle_Deg);  // Drive azimuth motor.
    DriveElevationMotor(nHeadElevationAngle_Deg);  // Drive elevation motor.

    // Simple state machine with timer.
    unCounter = unCounter + 1;  // Global counter.  Counts how many times the system executes.
    nTimer = nTimer - 1;
    if (nTimer == 0)        // Only execute state machine if timer is 0.
    {
      switch (nState)
      {
        case 0: // Initialize autonomous mode.
        nMoveMode = _FORWARD;
        nState = 1;
        nTimer = 20;
        break;

        case 1: // Scan data from machine vision module.
        if (nRow1 > 6)    // Check if front is blocked by obstacle.
        {
          nMoveMode = _STOP;
          nState = 2;
          nTimer = 4;
        }
        else if ((nRow1 == 4) || (nRow1 == 6))  // Check if obstacle on the left.
        {
          nMoveMode = _RIGHT;
          nState = 1;
          nTimer = 1;
        }
        else if ((nRow1 == 1) || (nRow1 == 3))  // Check if obstacle on the right.
        {
          nMoveMode = _LEFT;
          nState = 1;
          nTimer = 1;         
        }
        else  // No obstacle.
        {
          nMoveMode = _FORWARD;
          nState = 1;
          nTimer = 1;
        }     
        break;

        case 2: // Obstacle in front, reverse then turn.
        nMoveMode = _REVERSE;
        if (unCounter & 0x0001) // Randomly turn right or left
        {                       // depending on the last bit of nCounter!
          nState = 3;
        }
        else
        {
          nState = 4;
        }
        nTimer = 10;        
        break;

        case 3: // Turn right for a short while.
        nMoveMode = _RIGHT;
        nState = 1;
        nTimer = 6;        
        break;

        case 4: // Turn left for a short while.
        nMoveMode = _LEFT;
        nState = 1;
        nTimer = 6;
        break;
        
        default:
        nState = 0;
        nTimer = 1;
        break;
      } // End switch
    } // if (nTimer == 0)

    SetHBridgeMotorDriver(nMoveMode, nSpeed);   // Drive DC motors.
    
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

// Author    : Fabian Kung
// Date      : 19 August 2014
// Subroutines for driving the H-Bridge DC Motor Driver
// Arguments  : nMode - Movement mode
//              nSpeed - Speed setting, 0 to 10. 
//              0 corresponds to stop, while 10
//              is the fastest.
// Return     : None
// Example of usage:
//
// SetHBridgeMotorDriver(_FORWARD, 3);
// This will set the motors to move the robot forward and speed rating 3.
//
void SetHBridgeMotorDriver(int nMode, int nSpeed)
{
  analogWrite(pMotorLSpeedCtrl,15*nSpeed);
  analogWrite(pMotorRSpeedCtrl,15*nSpeed);
  
  if (nMode == _FORWARD)
  {
    digitalWrite(pMotorL1, HIGH);
    digitalWrite(pMotorL2, LOW);
    digitalWrite(pMotorR1, HIGH);
    digitalWrite(pMotorR2, LOW);
  }
  else if (nMode == _REVERSE)
  {
    digitalWrite(pMotorL1, LOW);
    digitalWrite(pMotorL2, HIGH);
    digitalWrite(pMotorR1, LOW);
    digitalWrite(pMotorR2, HIGH);
  }
  else if (nMode == _LEFT)
  {
    digitalWrite(pMotorL1, LOW);
    digitalWrite(pMotorL2, HIGH);
    digitalWrite(pMotorR1, HIGH);
    digitalWrite(pMotorR2, LOW);
  }
  else if (nMode == _RIGHT)
  {
    digitalWrite(pMotorL1, HIGH);
    digitalWrite(pMotorL2, LOW);
    digitalWrite(pMotorR1, LOW);
    digitalWrite(pMotorR2, HIGH);
  }
  else // Stop
  {
    digitalWrite(pMotorL1, 0);
    digitalWrite(pMotorL2, 0);
    digitalWrite(pMotorR1, 0);
    digitalWrite(pMotorR2, 0);
  }
}
