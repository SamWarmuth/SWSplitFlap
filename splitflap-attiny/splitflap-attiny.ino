/*
 *
var        ATTiny    TPIC
----------------------------
clockPin   13 SCK    SRCK   
latchPin   10 SS     RCK
dataPin    17 MOSI   SER IN
enablePin   9 GPIO   G (is NOT ground)

*/



#include <SPI.h>
#include <Wire.h>

byte chipID = 8; // any number from 8-127

static const int spiClk = 1000000; // in KHz

int pinOrder[4] = {0, 1, 2, 3};
int pinOrder2[4] = {3, 2, 1, 0};

bool steps[8][4] = {
  {0, 0, 0, 1},
  {0, 0, 1, 1},
  {0, 0, 1, 0},
  {0, 1, 1, 0},
  {0, 1, 0, 0},
  {1, 1, 0, 0},
  {1, 0, 0, 0},
  {1, 0, 0, 1}  
};

int outputSize = 8; //make sure this matches outputBytes below.
byte outputBytes[8];

int clockPin = 13; //vspi clk
int latchPin = 10; //ss or vspi CS0 pin
int dataPin = 17;  //VSPI MOSI. 
int enablePin = 9; //just a random gpio, controls TPIC on/off

int ledPin = 0; //turn on LED when moving

int hallPin = 2;     // the number of the hall effect sensor pin

int stepsPerRevolution = 4096;
//This number depends on the motor
int stepsPerFlap = 256;

float revolutionsPerMinute = 20.0;  //20 is a good start. Higher and you start skipping steps.
float stepsPerSecond = (revolutionsPerMinute/60.0) * (float)stepsPerRevolution;
float secondsPerStep = 1.0/stepsPerSecond;
int microsecondsPerStep = (int)(secondsPerStep * 1000000.0);

//10 rpm ~1,500 uS/step

const int startInterval = microsecondsPerStep * 2;
int interval = startInterval; //in uS
const int goalInterval = microsecondsPerStep;

void setup() {
  Wire.begin(chipID);
  Wire.onReceive(receiveMessage); // register event
  Wire.onRequest(sendMessage);
  
  pinMode(hallPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallPin), hallInterrupt, FALLING);
  SPI.begin();
  pinMode(ledPin, OUTPUT);

  pinMode(latchPin, OUTPUT);
  digitalWrite (latchPin, LOW);
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW);

  //loop through each array, then loop to create byte
  for (int index = 0; index < 8; index++) {
    byte combined = 0;
    for (int bitIndex = 0; bitIndex < 4; bitIndex++) {
      if (steps[index][bitIndex]) {
        bitSet(combined, pinOrder[bitIndex]);
        bitSet(combined, pinOrder2[bitIndex] + 4);
      }
    }
    outputBytes[index] = combined;
  }
  goToLocation(stepsPerRevolution - 1); //max distance, 1 step behind where we started
}

int currentLocation = 0;
int goalLocation = 0;
void goToLocation(int location) {
  goalLocation = location;
}

int stepIndex = 0;
unsigned long lastStepTime = 0;

int totalFlaps = 0;
int goalFlap = 0; //0 - 15

bool isOff = true;
bool ledState = LOW;
void loop() {
  if (goalLocation != currentLocation) {
    isOff = false;
    unsigned long now = micros();
    if (now - lastStepTime > interval) {
      currentLocation = (currentLocation + 1) % stepsPerRevolution;
      updateShiftRegister(outputBytes[stepIndex]);
      lastStepTime = now;
      stepIndex = (stepIndex + 1) % outputSize;
      if (interval > goalInterval) {
        interval -= 3;
      }
    }
  } else if (!isOff) {
    interval = startInterval;
    isOff = true;
    turnOffStepper();
  }
}

void receiveMessage(int howMany) {
  if (Wire.available() < 3) return;
  int type = Wire.read(); //ignore, always move for now
  int high = (int)Wire.read(); // Store as int to prepare for shifting
  byte low = Wire.read();
  
  goToLocation((high << 8) + low);
}

void sendMessage() {
  Wire.write("ATTiny");
}

//can be anything from 0 - 4096 (stepsPerRevolution)
int hallOffset = 12 * stepsPerFlap - 10; 

void hallInterrupt() {
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
  currentLocation = hallOffset;
}


void turnOffStepper() {
  byte off = 0;
  //updateShiftRegister(off);
}

void updateShiftRegister(byte output) {
  digitalWrite(latchPin, LOW); //pull SS slow to prep other end for transfer
  SPI.beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  SPI.transfer(output);
  SPI.endTransaction();
  digitalWrite(latchPin, HIGH); //pull ss high to signify end of data transfer
}
