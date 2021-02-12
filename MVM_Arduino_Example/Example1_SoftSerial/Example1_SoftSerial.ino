// This examples uses Software Serial port to communicate with MVM V1.5C.
// IMPORTANT:
// I observed that Software Serial cannot support baud rate higher that 19200 bps
// well on most basic Arduino boards such as UNO or Nano which uses Atmega 328P
// running at 16 MHz clock. This could be because Software Serial routines uses
// software approach to time the bits in each serial data packet. Thus at higher
// baud rate such as 38400, 57600 and 115200 bps, there is high error rate when 
// transmitting and receiving serial data using Software Serial, the MVM cannot
// intercept the command reliably from Arduino. If this happens,
// then revert back to hardware serial, which from experiments, can support up to
// 115200 bps baud rate.
 
#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(57600);      // Hardware serial at 57600 bps.
  //mySerial.begin(19200);    // Earlier versions of MVM V1.5C firmware uses lower baud rate.
  //mySerial.begin(38400);    
  mySerial.begin(57600);    // Software serial at 57600 bps. NOTE: see comments above.
                            // If this code can't work properly, most likely is because
                            // higher baud rate is not supported well by Software serial.
                            // In this case please revert back to using hardware serial
                            // port to commnicate with MVM V1.5C. This means we can no
                            // longer send data to Serial Monitor.
  delay(2000);              // A long ms delay for MVM to initialize properly.
  //mySerial.write(0x30);     // Run IPA (image processing algorithm) 3 on interval 1.
  //mySerial.write(0x30);     // Run IPA 3 on interval 2.
  //mySerial.write(0x20);   // Run IPA 2 on interval 2.
  //mySerial.write(0x20);   // Run IPA 2 on interval 2.
  mySerial.write(0x10);   // Run IPA 1 on interval 2.
  mySerial.write(0x10);   // Run IPA 1 on interval 2. 
}

void loop() {
  // put your main code here, to run repeatedly:

int nID;
int nLuminance;
int nX;
int nY;
int nParam1;
int nParam2;
int nParam3;

  if (mySerial.available()>3)
  {
    nID = mySerial.read();        // Get IPA ID.
    
    if (nID == 1)                 // Make sure it is image processing algorithm 1.
    {
      nLuminance = mySerial.read();
      nX = mySerial.read();
      nY = mySerial.read();
      Serial.print("Process ID = ");
      Serial.print(nID);
      Serial.println();
      Serial.print("X coordinate = ");
      Serial.print(nX);
      Serial.println();
      Serial.print("Y coordinate = ");
      Serial.print(nY);
      Serial.println();
    }
    else
    {
      mySerial.flush();
    }
  }
  delay(2);       // A short 2 ms delay.
}
