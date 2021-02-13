void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  delay(1000);          // A 1000 ms delay for the module to initialize properly.
  Serial.write(0x10);   // Run IPA (image processing algorithm) 1 on interval 1.
  Serial.write(0x20);   // Run IPA 2 on interval 2.
}

void loop() {
  // put your main code here, to run repeatedly:

int nID, nLuminance, nX, nY;
int nRow0, nRow1, nRow2;

  if (Serial.available() > 3) // Make sure at least 4 bytes in the received buffer.
  {
    nID = Serial.read();      // Get process ID.
    if (nID == 1)             // Make sure the data is from IPA 1.
    {
      nLuminance = Serial.read();
      nX = Serial.read();
      nY = Serial.read();
    }
    else if (nID == 2)       // Make sure the data is from IPA 2.
    {
      nRow2 = Serial.read();
      nRow1 = Serial.read();
      nRow0 = Serial.read();
    }
    else
    {
      Serial.flush();     // Flush received buffer.
    }
  }
}
