/*
esp32 default pins:

22: SCL
21: SDA

*/
#include <Wire.h>

int tinyID = 8;

void setup() {
  Wire.begin();
  Serial.begin(115200); 
  Serial.println("Started Up");
}

byte action[3] = {
  0, // move to position
  1, // set zero-point
  2 // set speed
};

byte flapIndex = 0;
void loop() {
  Serial.print("Move to ");
  Wire.beginTransmission(tinyID);   //max 32 bytes per transmission!
  Wire.write(action[0]);              // "Move"
  int location = flapIndex * 256; //max 4095
  Serial.println(location);
  Wire.write((location & 0xFF00) >> 8);  //high bytes -- always send both, even if <255
  Wire.write(location & 0x00FF);         // low bytes
  Wire.endTransmission();    // stop transmitting
  flapIndex = (flapIndex + 1) % 11;
  delay(200);
  Wire.requestFrom(tinyID, 12);
  while (Wire.available()) { // slave may send less than requested
    char c = Wire.read(); // receive a byte as character
    Serial.print(c);         // print the character
  }
  Serial.println(", done.");
  delay(1000);
}
