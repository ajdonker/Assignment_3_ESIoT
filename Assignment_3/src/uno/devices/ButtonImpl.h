#ifndef __BUTTONIMPL__
#define __BUTTONIMPL__

#include "Button.h"
#include <Arduino.h>
class ButtonImpl: public Button {
 
public: 
  ButtonImpl(uint8_t pin);
  bool isPressed();

private:
  uint8_t pin;

};

#endif
