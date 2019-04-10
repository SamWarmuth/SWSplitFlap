/*
var        ESP  shift  TPIC
----------------------------
clockPin   18   LV1    SRCK   
latchPin   5    LV2    RCK
dataPin    23   LV3    SER IN
enablePin  19   LV4    G (is NOT ground)
*/



#include <SPI.h>
static const int spiClk = 500000; // in KHz

int pinOrder[4] = {1, 2, 0, 3};
// 1 2 0 3 wooooooooooooo!

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


byte outputBytes[8];

int clockPin = 18; //vspi clk
int latchPin = 5; //ss or vspi CS0 pin
int dataPin = 23;  //VSPI MOSI. 
int enablePin = 19; //just a random gpio

int hallPin = 32;     // the number of the hall effect sensor pin
int stepsPerRevolution = 4096;
//This number depends on the motor
int stepsPerFlap = 256;

float revolutionsPerMinute = 15.0;  //about 18rpm is the max, 16 is a good pick
float stepsPerSecond = (revolutionsPerMinute/60.0) * (float)stepsPerRevolution;
float secondsPerStep = 1.0/stepsPerSecond;
int microsecondsPerStep = (int)(secondsPerStep * 1000000.0);

//10 rpm ~1,500 uS

int goalInterval = microsecondsPerStep; //in uS
int initialInterval = goalInterval; //Something weird happened with acc, turn off for now  
int currentInterval = initialInterval;
SPIClass * vspi = NULL;

void setup() {
  Serial.begin(115200);
  Serial.print("uSPS: ");
  Serial.println(microsecondsPerStep);

  pinMode(hallPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallPin), hallInterrupt, FALLING);

  vspi = new SPIClass(VSPI);
  vspi->begin();

  //still neet to set and use latch
  pinMode(latchPin, OUTPUT);
  digitalWrite (latchPin, LOW);
  
//  pinMode(latchPin, OUTPUT);
//  pinMode(clockPin, OUTPUT);
//  pinMode(dataPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
//
//  digitalWrite(latchPin, HIGH);
//  digitalWrite(clockPin, LOW);
//  digitalWrite(dataPin, LOW);
  digitalWrite(enablePin, LOW);

  
  //loop through each array, then loop to create byte
  for (int index = 0; index < 8; index++) {
    byte combined = 0;
    for (int bitIndex = 0; bitIndex < 4; bitIndex++) {
      if (steps[index][bitIndex]) {
        bitSet(combined, pinOrder[bitIndex]);
      }
    }
    outputBytes[index] = combined;
  }

  Serial.println("Started Up.");
  goToFlap(15);
}


int stepIndex = 0;
unsigned long lastStepTime = 0;
unsigned long totalSteps = 0;

int totalFlaps = 0;
int goalFlap = 0; //0 - 15

int serialIn = 0;
void loop() {
  moveIfNecessary();
  int goalSteps = getGoalSteps();
  if (goalSteps == totalSteps && Serial.available() > 0) {
    serialIn = Serial.parseInt();
    if (serialIn != 0){
      
      goToFlap(serialIn);
    }
    // say what you got:
    Serial.print("I received: ");
    Serial.println(serialIn);

  }
}

//can be anything from 0 - 4096 (stepsPerRevolution), but will have absolute value issues near edges.
int hallPosition = 10 * stepsPerFlap - 10; 


int lastHall = 0;
int skippedSteps = 0;
void hallInterrupt() {
  int location = totalSteps % stepsPerRevolution;
  Serial.println(location);
  if (location !=  hallPosition) {
    int diff = location - hallPosition;
    Serial.print("Adjust ");
    Serial.println(diff);
    totalSteps = totalSteps - diff;
  }
  lastHall = totalSteps;
}

int startStep;
void goToFlap(int flapIndex) {
  goalFlap = flapIndex % 16;
  startStep = totalSteps;
}



int getGoalSteps() {
  int revolutionCount = totalSteps / stepsPerRevolution;
  int currentRotation = totalSteps % stepsPerRevolution;
  int goalRotation = goalFlap * stepsPerFlap;
  if (goalRotation + 10 < currentRotation) { //give a ten-step margin to allow for a little overshoot.
    goalRotation = goalRotation + stepsPerRevolution;
  }
  return revolutionCount * stepsPerRevolution + goalRotation;
}

bool isOff = true;
void moveIfNecessary() {
  int goalSteps = getGoalSteps();

  if (totalSteps < goalSteps) {
    isOff = false;
    unsigned long now = micros();
    if (now - lastStepTime > currentInterval) {
      if (startStep == totalSteps) {
        Serial.print("Ok, start move. Going from step ");
        Serial.print(startStep);
        Serial.print(" to step ");
        Serial.print(goalSteps);
        Serial.print(" (");
        Serial.print(goalSteps - startStep);
        Serial.println(" steps)");
      }
      totalSteps = totalSteps + 1;
      updateShiftRegister(outputBytes[stepIndex]);
      lastStepTime = now;
      stepIndex = (stepIndex + 1) % sizeof(outputBytes);

      if (currentInterval > goalInterval) {
        currentInterval = (int)((float)currentInterval * 0.98);
        if (currentInterval < goalInterval) {
          currentInterval = goalInterval;
        }
      }
    }
  } else if (!isOff) {
    //wait a while (I picked 5 intervals) to make sure that motor has stopped moving.
    unsigned long now = micros();
    if (now - lastStepTime > currentInterval * 5) {
      turnOffStepper();
      currentInterval = initialInterval;
      isOff = true;   
    }
     
  }
}

void turnOffStepper() {
  byte off = 0;
  updateShiftRegister(off);
}

void updateShiftRegister(byte output) {
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(latchPin, LOW); //pull SS slow to prep other end for transfer
  vspi->transfer(output);  
  digitalWrite(latchPin, HIGH); //pull ss high to signify end of data transfer
  vspi->endTransaction();
}
