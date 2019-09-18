#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  mySerial.begin(57600);
  delay(500);           // A 500 ms delay for the module to initialize properly.
  mySerial.write(0x10);   // Run IPA (image processing algorithm) 1 on interval 1.
  mySerial.write(0x10);   // Run IPA 1 on interval 2.
}

void loop() {
  // put your main code here, to run repeatedly:

int nID;
int nLuminance;
int nX;
int nY;


  if (mySerial.available()>3)
  {
    nID = mySerial.read();        // Get IPA ID.
    if (nID == 1)                 // Make sure it is algorithm 1.
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
