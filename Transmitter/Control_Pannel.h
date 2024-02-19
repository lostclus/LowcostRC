#ifndef Control_Pannel_h
#define Control_Pannel_h

#include "Config.h"

#if defined(WITH_ADAFRUIT_SSD1306)
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#elif defined(WITH_SSD1306_ASCII)
#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#endif

#include <AbleButtons.h>
#include <LowcostRC_Protocol.h>
#include "Buzzer.h"
#include "Radio_Control.h"
#include "Settings.h"
#include "Controls.h"

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

enum Screen {
  NO_SCREEN,
  SCREEN_BATTARY,
  SCREEN_PROFILE,
  SCREEN_RF_CHANNEL,
  SCREEN_AUTO_CENTER,
  SCREEN_DUAL_RATE_A_X,
  SCREEN_DUAL_RATE_A_Y,
  SCREEN_DUAL_RATE_B_X,
  SCREEN_DUAL_RATE_B_Y,
  SCREEN_TRIMMING_A_X,
  SCREEN_TRIMMING_A_Y,
  SCREEN_TRIMMING_B_X,
  SCREEN_TRIMMING_B_Y,
  SCREEN_INVERT_A_X,
  SCREEN_INVERT_A_Y,
  SCREEN_INVERT_B_X,
  SCREEN_INVERT_B_Y,
  SCREEN_CHANNEL_A_X,
  SCREEN_CHANNEL_A_Y,
  SCREEN_CHANNEL_B_X,
  SCREEN_CHANNEL_B_Y,
  SCREEN_LOW_SWITCH_1,
  SCREEN_LOW_SWITCH_2,
  SCREEN_HIGH_SWITCH_1,
  SCREEN_HIGH_SWITCH_2,
  SCREEN_CHANNEL_SWITCH_1,
  SCREEN_CHANNEL_SWITCH_2,
  SCREEN_BATTARY_LOW,
  SCREEN_SAVE_FOR_NOLINK,
  SCREEN_SAVE,
  FIRST_SCREEN = NO_SCREEN,
  LAST_SCREEN = SCREEN_SAVE,
};

class ControlPannel {
  private:
    bool settingsLongPress = false;
    Screen screenNum = NO_SCREEN;
    Settings *settings;
    Buzzer *buzzer;
    RadioControl *radioControl;
    Controls *controls;
    unsigned int thisBattaryMV = 0;

#if defined(WITH_ADAFRUIT_SSD1306)
    Adafruit_SSD1306 display;
#elif defined(WITH_SSD1306_ASCII)
    SSD1306AsciiWire display;
#endif

    Button settingsButton,
           settingsPlusButton,
           settingsMinusButton;
    Button *buttonsArray[3];
    ButtonList buttons;
    unsigned long battaryUpdateTime = 0;
    struct TelemetryPacket telemetry;

    void redrawScreen();
  public:
    ControlPannel(
      Settings *settings,
      Buzzer *buzzer,
      RadioControl *radioControl,
      Controls *controls
    );
    void begin();
    void handle();
};

#endif // Control_Pannel_h
// vim:ai:sw=2:et
