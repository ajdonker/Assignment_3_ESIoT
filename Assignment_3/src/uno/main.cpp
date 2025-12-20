#include<Arduino.h>
#include "devices/Button.h"
#include "devices/ButtonImpl.h"
#include "devices/ServoMotor.h"
#include "devices/ServoMotorImpl.h"
#include "devices/Pot.h"
#include "LiquidCrystal_I2C.h"
#include <Wire.h>
#define TRIG_PIN 9
#define ECHO_PIN 10
#define MOTOR_PIN 5
#define POT_PIN A2
#define BUTTON_PIN 2

bool lastButtonState = false;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 50;

const float L1 = 0.2;
const float L2 = 0.3;
const int T1 = 3000;
const int T2 = 5000;
float waterLevel = -1.0;
unsigned long belowL1Timestamp = 0;
unsigned long lastRecvTime = 0;
bool buttonPressed = false;
enum class WcsState{AUTOMATIC,MANUAL,UNCONNECTED} state;
typedef void (*wcsStateChangedCallback)(WcsState);
wcsStateChangedCallback onWcsStateChanged;
Button *pButton;
bool buttonPressedOnce() {
  static bool last = false;
  bool now = pButton->isPressed();

  if (now && !last) {
    last = now;
    return true;
  }

  last = now;
  return false;
}
ServoMotor *pMotor;
Potentiometer *pPot;
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27,20,4); 
void onWcsStateChangedHandler(WcsState s){
  lcd.clear();
  lcd.print("STATE:");
  switch (s)
  {
  case WcsState::MANUAL:
    lcd.print("MANUAL");
    Serial.println("MANUAL");
    //Serial.flush();
    break;
  case WcsState::UNCONNECTED:
    lcd.print("UNCONNECTED");
    Serial.println("UNCONNECTED");
    //Serial.flush();
    break;
  default:
    lcd.print("AUTOMATIC");
    Serial.println("AUTOMATIC");
    //Serial.flush();
    break;
  } 
}
void setState(WcsState newState) {
  if (state != newState) {
    state = newState;
    if (onWcsStateChanged) {
      onWcsStateChanged(state);
    }
  }
}
void handleSerial()
{
  if (!Serial.available()) return;

  String msg = Serial.readStringUntil('\n');
  msg.trim();
  Serial.print("Msg received:");
  Serial.println(msg);
  //Serial.flush();
  if (msg.startsWith("DIST:")) {
    waterLevel = msg.substring(5).toFloat();
    lcd.setCursor(2,2);
    lcd.print("Water level:");
    lcd.print(waterLevel);
    lastRecvTime = millis();

    if (state == WcsState::UNCONNECTED) {
      setState(WcsState::AUTOMATIC);
    }
  }
  else if (msg == "UNCONNECTED") {
    setState(WcsState::UNCONNECTED);
  }
  //lcd.clear();
  // lcd.print("State:");
  // switch (state)
  // {
  // case WcsState::MANUAL:
  //   /* code */
  //   lcd.print("MANUAL");
  //   break;
  // case WcsState::UNCONNECTED:
  //   lcd.print("UNCONNECTED");
  //   /* code */
  //   break;
  // default:
  //   lcd.print("AUTOMATIC");
  //   break;
  // }
}
void setup() {
  Wire.begin();
  Serial.begin(115200);
  pButton = new ButtonImpl(BUTTON_PIN);
  pMotor = new ServoMotorImpl(MOTOR_PIN);
  pPot = new Potentiometer(POT_PIN);
  pPot->sync();
  pMotor->on();
  pMotor->setPosition(0);
  setState(WcsState::AUTOMATIC);
  onWcsStateChanged = onWcsStateChangedHandler;
  lcd.clear();
  lcd.init();
  lcd.backlight();
  lcd.print("INIT");
  Serial.println("setup complete");
}
void loop() {
  
  handleSerial();
  unsigned long now = millis();
  // if(now - lastRecvTime > T2)
  // {
  //   setState(WcsState::UNCONNECTED);
  // }
  if(waterLevel > 0)
  {
    switch(state)
    {
      case WcsState::AUTOMATIC:
        if(waterLevel < L1){
          pMotor->setPosition(0); 
        }
        else if (waterLevel < L2)
        {
          pMotor->setPosition(45); 
        }
        else
        {
          pMotor->setPosition(90); 
        }
        
        if(buttonPressedOnce())
        {
          setState(WcsState::MANUAL);
        }
      break;
      case WcsState::MANUAL:
        pPot->sync();
        float potReadout = pPot->getValue();
        //Serial.print("POT READOUT:");
        //Serial.println(potReadout);
        // convert pot readouts into range 0-90 degrees (0-100%)
        pMotor->setPosition(potReadout);
        if(buttonPressedOnce())
        {
          setState(WcsState::AUTOMATIC);
        }
      break;
      case WcsState::UNCONNECTED:
        // to be set in the CUS through Serial messaging 
        pMotor->setPosition(0);
      break;
    }
  }
}