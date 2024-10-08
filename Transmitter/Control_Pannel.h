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
#include <LowcostRC_VoltMetter.h>
#include "Buzzer.h"
#include "Radio_Control.h"
#include "Settings.h"
#include "Controls.h"

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

enum ScreenEnum {
  SCREEN_NULL,
  SCREEN_BLANK,
  SCREEN_DISPLAY,
  SCREEN_PROFILE,
  SCREEN_PROFILE_NAME,

  // Radio
  SCREEN_BIND_PEER,
  SCREEN_PEER_ADDR,
  SCREEN_RF_CHANNEL,
  SCREEN_PA_LEVEL,

  // Controls
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
  SCREEN_LOW_SWITCH_1,
  SCREEN_LOW_SWITCH_2,
  SCREEN_LOW_SWITCH_3,
  SCREEN_LOW_SWITCH_4,
  SCREEN_HIGH_SWITCH_1,
  SCREEN_HIGH_SWITCH_2,
  SCREEN_HIGH_SWITCH_3,
  SCREEN_HIGH_SWITCH_4,

  // Mapping
  SCREEN_CHANNEL_A_X,
  SCREEN_CHANNEL_A_Y,
  SCREEN_CHANNEL_B_X,
  SCREEN_CHANNEL_B_Y,
  SCREEN_CHANNEL_SWITCH_1,
  SCREEN_CHANNEL_SWITCH_2,
  SCREEN_CHANNEL_SWITCH_3,
  SCREEN_CHANNEL_SWITCH_4,

  // Peer
  SCREEN_BATTERY_LOW,
  SCREEN_SAVE_FAILSAFE,

  SCREEN_SAVE,

#ifdef FLAT_MENU
  FIRST_SCREEN = SCREEN_BLANK,
  LAST_SCREEN = SCREEN_SAVE,
#else
  SCREEN_GROUP_RADIO,
  SCREEN_GROUP_CONTROLS,
  SCREEN_GROUP_MAPPING,
  SCREEN_GROUP_PEER,
  SCREEN_MENU_UP,
#endif
};

typedef uint8_t Screen;

class ControlPannel {
  private:
    Settings *settings;
    Buzzer *buzzer;
    RadioControl *radioControl;
    Controls *controls;
#ifdef FLAT_MENU
    Screen currentScreen;
#else
    Screen *menuStack[2],
           **currentMenu,
           *currentMenuItem;
#define currentScreen (*currentMenuItem)
#endif

#if defined(WITH_ADAFRUIT_SSD1306)
    Adafruit_SSD1306 display;
#elif defined(WITH_SSD1306_ASCII)
    SSD1306AsciiWire display;
#endif

    Button screenButton,
           plusButton,
           minusButton;
    VoltMetter voltMetter;
    unsigned long batteryUpdateTime = 0,
                  settingsChangeTime = 0,
                  redrawTime = 0;
    unsigned int thisBatteryMV = 0;
    uint8_t flags = 0,
            cursor = 0;

    void redrawScreen();
    void moveMenuTop();
    void moveMenuForward();
#ifndef FLAT_MENU
    void moveMenuUp();
    void moveMenuDown();
#endif
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
