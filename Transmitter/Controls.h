#ifndef Controls_h
#define Controls_h

#include "Types.h"
#include "Buzzer.h"
#include "Radio_Control.h"
#include "Settings.h"

class Controls {
  private:
    Settings *settings;
    Buzzer *buzzer;
    RadioControl *radioControl;
  public:
    Controls(Settings *settings, Buzzer *buzzer, RadioControl *radioControl);
    void begin();
    void setJoystickCenter();
    int mapAxis(
      int joyValue,
      int joyCenter,
      int joyThreshold,
      bool joyInvert,
      int dualRate,
      int trimming
    );
    int readAxis(Axis axis);
    int readSwitch(Switch sw);
    void handle();
};

#endif // Controls_h
// vim:ai:sw=2:et
