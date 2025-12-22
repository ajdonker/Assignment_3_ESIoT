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

// to prevent brownout that happened 
// motor moves degree per degree to the point needed 100 ms a time 
float waterLevel = -1.0;
unsigned long belowL1Timestamp = 0;
unsigned long lastRecvTime = 0;
bool buttonPressed = false;
float lastRemoteMotorPosition = 0.0;
enum class WcsState{AUTOMATIC,MANUAL,UNCONNECTED,NOT_AVAILABLE} state;
enum class Control{POT,REMOTE} control;
typedef void (*wcsStateChangedCallback)(WcsState);
wcsStateChangedCallback onWcsStateChanged;
Button *pButton;
bool isPressedOnce() {
  static bool last = false;
  bool now = pButton->isPressed();
  //Serial.println("interrupt");
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

__FlashStringHelper* stateToFString(WcsState s) {
  switch (s) {
    case WcsState::MANUAL:
      return F("MANUAL");
    case WcsState::UNCONNECTED:
      return F("UNCONNECTED");
    case WcsState::NOT_AVAILABLE: 
      return F("NOT_AVAILABLE");
    default:
      return F("AUTOMATIC");
  }
}
WcsState stringToState(const String& s) {
  if (s.equalsIgnoreCase("AUTOMATIC")) return WcsState::AUTOMATIC;
  if (s.equalsIgnoreCase("MANUAL")) return WcsState::MANUAL;
  if (s.equalsIgnoreCase("UNCONNECTED")) return WcsState::UNCONNECTED;
  return WcsState::NOT_AVAILABLE;
  
  return WcsState::UNCONNECTED; 
}
void onWcsStateChangedHandler(WcsState s){
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("STATE:");
  Serial.print("STATE:");
  switch (s)
  {
  case WcsState::MANUAL:
    lcd.print("MANUAL");
    Serial.println("MANUAL");
    Serial.flush();
    break;
  case WcsState::UNCONNECTED:
    lcd.print("UNCONNECTED");
    Serial.println("UNCONNECTED");
    Serial.flush();
    break;
  case WcsState::NOT_AVAILABLE:
    lcd.print("NOT_AVAILABLE");
    // Serial.println("UNCONNECTED");
    // Serial.flush();
    break;
  default:
    lcd.print("AUTOMATIC");
    Serial.println("AUTOMATIC");
    Serial.flush();
    break;
  }
  if(state != WcsState::UNCONNECTED && state != WcsState::NOT_AVAILABLE)
  {
    lcd.setCursor(2,2);
    lcd.print("Water Level:"); 
    lcd.print(waterLevel);
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
unsigned long moveStartTime = 0;
int startPos = 0;
int targetPos = 0;
float currentPos = 0.0f;
int desiredPos;
bool motorMoving = false;
const int MOTOR_MAX_MOVE_TIME = 3000;
void startMotorMove(int newTarget) {
  if (newTarget == targetPos) return;   // prevent retrigger

  startPos = currentPos;
  targetPos = newTarget;
  moveStartTime = millis();
  motorMoving = true;
}
void updateMotor() {
  if (!motorMoving) return;

  unsigned long dt = millis() - moveStartTime;

  if (dt >= MOTOR_MAX_MOVE_TIME) {
    currentPos = targetPos;
    motorMoving = false;
  } else {
    float t = (float)dt / MOTOR_MAX_MOVE_TIME;
    currentPos = startPos + t * (targetPos - startPos);
  }

  pMotor->setPosition(currentPos);
}
void handleSerial()
{
  if (!Serial.available()) return;
  String msg = Serial.readStringUntil('\n');
  if (msg.length() == 0) return; 
  msg.trim();
  // this doesnt get parsed, remove 
  // Serial.print("Msg received:");
  // Serial.println(msg);
  //Serial.flush();
  if (msg.startsWith("DIST:")) {
    waterLevel = msg.substring(5).toFloat();
    lcd.setCursor(2,2);
    lcd.print("Water level:");
    lcd.print(waterLevel);
    Serial.print("LEVEL:");
    Serial.println(waterLevel);
    lastRecvTime = millis();

    if (state == WcsState::UNCONNECTED) {
      setState(WcsState::AUTOMATIC);
    }
    if (state == WcsState::NOT_AVAILABLE) {
      setState(WcsState::AUTOMATIC);
    }
  }
  else if (msg.startsWith("MODE:")){
    String stateStr = msg.substring(5);
    stateStr.trim(); 
    lcd.clear();
    WcsState newState = stringToState(stateStr);
    setState(newState); // callback prints to serial 
  }
  else if(msg.startsWith("MOTOR:")){
    // check if control is to the dbs or here 
    int pos = msg.substring(6).toInt();
    pos = constrain(pos,0,90);
    if(state == WcsState::MANUAL && control == Control::REMOTE)
    {
      lastRemoteMotorPosition = pos;
      Serial.print("MOTOR:");
      Serial.println(lastRemoteMotorPosition);
      Serial.flush();
    }
    else
    {
      Serial.println("AUTO MODE OR CONTROL NOT GAINED.MOTOR IGNORED.");
      Serial.flush();
    }
  }
  else if(msg.startsWith("CONTROL:")){
    String who = msg.substring(8);
    who.trim();
    if(who.equalsIgnoreCase("POT")){
      control = Control::POT;
      Serial.println("CONTROL:POT");
    }
    else if(who.equalsIgnoreCase("REMOTE")){
      control = Control::REMOTE;
      Serial.println("CONTROL:REMOTE");
    }
    Serial.flush();
  }
}
void setup() {
  Wire.begin();
  Serial.begin(115200);
  pButton = new ButtonImpl(BUTTON_PIN);
  pMotor = new ServoMotorImpl(MOTOR_PIN);
  pPot = new Potentiometer(POT_PIN);
  pPot->sync();
  pMotor->on();
  lcd.init();
  lcd.backlight();
  //lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("STATE:");
  currentPos = 0;
  pMotor->setPosition(0);
  control = Control::POT;
  onWcsStateChanged = onWcsStateChangedHandler;
  setState(WcsState::AUTOMATIC);
  lcd.print(stateToFString(state));
  lcd.setCursor(2,2);
  lcd.print("WATER LEVEL:");
  lcd.print(waterLevel);
  lastRecvTime = millis();
  Serial.println("setup complete");
}
void loop() {
  
  handleSerial();
  unsigned long now = millis();
  if (state != WcsState::NOT_AVAILABLE &&
  now - lastRecvTime > T2) {
    setState(WcsState::NOT_AVAILABLE);
  }
  if(waterLevel >= 0)
  {
    switch(state)
    {
      case WcsState::AUTOMATIC:
        if(waterLevel < L1){
          desiredPos = 0;
          belowL1Timestamp = now;
        }
        else if (waterLevel < L2)
        {
          if(now - belowL1Timestamp > T1)
          {
             desiredPos = 45;
          }
          else
          {
            desiredPos = 0;
          }
        }
        else
        {
          desiredPos = 90;
        }
        startMotorMove(desiredPos);
        updateMotor();
        if(isPressedOnce())
        {
          control = Control::POT;
          Serial.println("CONTROL:POT");
          setState(WcsState::MANUAL);
        }
      break;
      case WcsState::MANUAL:
        if(control == Control::POT)
        {
          pPot->sync();
          float potReadout = pPot->getValue();
          // float potReadout = 0.8;
          //pMotor->setPosition(potReadout);
          // Serial.println(potReadout);
          desiredPos = potReadout * 90; // pot readout goes from 0.0 to 1.0
          if(isPressedOnce())
          {
            control = Control::POT;
            Serial.println("CONTROL:POT");
            setState(WcsState::AUTOMATIC);
          }
        }
        else
        {
          // control by seting desired Pos as per what is received from the slider
          desiredPos = lastRemoteMotorPosition;
        }
        startMotorMove(desiredPos);
        updateMotor();
      break;
      case WcsState::UNCONNECTED:
        pMotor->setPosition(0);
      break;
    }
  }
}