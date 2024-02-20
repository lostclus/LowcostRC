#include <LowcostRC_Console.h>
#include "Battary.h"
#include "Control_Pannel.h"

const int DUAL_RATE_MIN = 10,
          DUAL_RATE_MAX = 1500,
          TRIMMING_MIN = -1500,
          TRIMMING_MAX = 1500,
          SWITCH_MIN = 0,
          SWITCH_MAX = 3000;

#ifndef FLAT_MENU
const Screen controlsMenu[] = {
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
  SCREEN_HIGH_SWITCH_1,
  SCREEN_HIGH_SWITCH_2,
  SCREEN_MENU_UP,
  SCREEN_NULL
};

const Screen mappingMenu[] = {
  SCREEN_CHANNEL_A_X,
  SCREEN_CHANNEL_A_Y,
  SCREEN_CHANNEL_B_X,
  SCREEN_CHANNEL_B_Y,
  SCREEN_CHANNEL_SWITCH_1,
  SCREEN_CHANNEL_SWITCH_2,
  SCREEN_MENU_UP,
  SCREEN_NULL
};

const Screen peerMenu[] = {
  SCREEN_BATTARY_LOW,
  SCREEN_SAVE_FOR_NOLINK,
  SCREEN_MENU_UP,
  SCREEN_NULL
};

const Screen mainMenu[] = {
  SCREEN_BLANK,
  SCREEN_BATTARY,
  SCREEN_PROFILE,
  SCREEN_RF_CHANNEL,
  SCREEN_GROUP_CONTROLS,
  SCREEN_GROUP_MAPPING,
  SCREEN_GROUP_PEER,
  SCREEN_SAVE,
  SCREEN_NULL
};

struct SubMenu {
  Screen screen,
         *subMenu;
} subMenu[] = {
  {SCREEN_GROUP_CONTROLS, controlsMenu},
  {SCREEN_GROUP_MAPPING, mappingMenu},
  {SCREEN_GROUP_PEER, peerMenu},
};
#endif

#define addWithConstrain(value, delta, lo, hi) value = constrain(value + (delta), lo, hi)

ControlPannel::ControlPannel(
  Settings *settings, Buzzer *buzzer, RadioControl *radioControl, Controls *controls
)
  : settings(settings)
  , buzzer(buzzer)
  , radioControl(radioControl)
  , controls(controls)
#if defined(WITH_ADAFRUIT_SSD1306)
  , display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1)
#elif defined(WITH_SSD1306_ASCII)
  , display()
#endif
  , settingsButton(SETTINGS_PIN)
  , settingsPlusButton(SETTINGS_PLUS_PIN)
  , settingsMinusButton(SETTINGS_MINUS_PIN)
  , buttonsArray({&settingsButton, &settingsPlusButton, &settingsMinusButton})
  , buttons(buttonsArray)
{
  moveMenuTop();
}

void ControlPannel::begin() {
  buttons.begin();
  settingsButton.setDebounceTime(20);
  settingsPlusButton.setDebounceTime(20);
  settingsMinusButton.setDebounceTime(20);

#if defined(WITH_ADAFRUIT_SSD1306)
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    PRINTLN(F("SSD1306 init FAIL"));
  } else {
    PRINTLN(F("SSD1306 init OK"));
  };
  display.clearDisplay();
  display.display();
#elif defined(WITH_SSD1306_ASCII)
  Wire.begin();
  Wire.setClock(400000L);
  display.begin(&Adafruit128x64, DISPLAY_ADDRESS);
  display.setFont(System5x7);
  display.clear();
#endif
}

void ControlPannel::redrawScreen() {
  char text[50] = "",
       yStr[] = "y",
       nStr[] = "n",
       axisNames[][3] = {"AX", "AY", "BX", "BY"},
       switchNames[][4] = {"SW1", "SW2"};
  Axis axis;
  Switch sw;

  switch (currentScreen) {
    case SCREEN_BLANK:
      break;
    case SCREEN_BATTARY:
      sprintf_P(
        text,
        PSTR("Battary\nT: %d.%03dV\nR: %d.%03dV"),
        thisBattaryMV / 1000,
        thisBattaryMV % 1000,
        telemetry.battaryMV / 1000,
        telemetry.battaryMV % 1000
      );
      break;
    case SCREEN_PROFILE:
      sprintf_P(
        text,
        PSTR("Profile\n%d"),
        settings->currentProfile
      );
      break;
    case SCREEN_RF_CHANNEL:
      sprintf_P(
        text,
        PSTR("RF channel\n%d"),
        settings->values.rfChannel
      );
      break;
    case SCREEN_AUTO_CENTER:
      sprintf_P(
        text,
        PSTR("J centers\n%d,%d,\n%d,%d"),
        settings->values.axes[AXIS_A_X].joyCenter,
        settings->values.axes[AXIS_A_Y].joyCenter,
        settings->values.axes[AXIS_B_X].joyCenter,
        settings->values.axes[AXIS_B_Y].joyCenter
      );
      break;
    case SCREEN_DUAL_RATE_A_X:
    case SCREEN_DUAL_RATE_A_Y:
    case SCREEN_DUAL_RATE_B_X:
    case SCREEN_DUAL_RATE_B_Y:
      axis = currentScreen - SCREEN_DUAL_RATE_A_X;
      sprintf_P(
        text,
        PSTR("D/R %s\n%d"),
        axisNames[axis],
        settings->values.axes[axis].dualRate
      );
      break;
    case SCREEN_TRIMMING_A_X:
    case SCREEN_TRIMMING_A_Y:
    case SCREEN_TRIMMING_B_X:
    case SCREEN_TRIMMING_B_Y:
      axis = currentScreen - SCREEN_TRIMMING_A_X;
      sprintf_P(
        text,
        PSTR("Tr %s\n%d"),
        axisNames[axis],
        settings->values.axes[axis].trimming
      );
      break;
    case SCREEN_INVERT_A_X:
    case SCREEN_INVERT_A_Y:
    case SCREEN_INVERT_B_X:
    case SCREEN_INVERT_B_Y:
      axis = currentScreen - SCREEN_INVERT_A_X;
      sprintf_P(
        text,
        PSTR("Invert %s\n%s"),
        axisNames[axis],
        settings->values.axes[axis].joyInvert ? yStr : nStr
      );
      break;
    case SCREEN_LOW_SWITCH_1:
    case SCREEN_LOW_SWITCH_2:
      sw = currentScreen - SCREEN_LOW_SWITCH_1;
      sprintf_P(
        text,
        PSTR("Low %s\n%d"),
        switchNames[sw],
        settings->values.switches[sw].low
      );
      break;
    case SCREEN_HIGH_SWITCH_1:
    case SCREEN_HIGH_SWITCH_2:
      sw = currentScreen - SCREEN_HIGH_SWITCH_1;
      sprintf_P(
        text,
        PSTR("High %s\n%d"),
        switchNames[sw],
        settings->values.switches[sw].high
      );
      break;
    case SCREEN_CHANNEL_A_X:
    case SCREEN_CHANNEL_A_Y:
    case SCREEN_CHANNEL_B_X:
    case SCREEN_CHANNEL_B_Y:
      axis = currentScreen - SCREEN_CHANNEL_A_X;
      if (settings->values.axes[axis].channel != NO_CHANNEL) {
        sprintf_P(
          text,
          PSTR("Channel %s\n%d"),
          axisNames[axis],
          settings->values.axes[axis].channel + 1
        );
      } else {
        sprintf_P(
          text,
          PSTR("Channel %s\nNone"),
          axisNames[axis]
        );
      }
      break;
    case SCREEN_CHANNEL_SWITCH_1:
    case SCREEN_CHANNEL_SWITCH_2:
      sw = currentScreen - SCREEN_CHANNEL_SWITCH_1;
      if (settings->values.switches[sw].channel != NO_CHANNEL) {
        sprintf_P(
          text,
          PSTR("Ch %s\n%d"),
          switchNames[sw],
          settings->values.switches[sw].channel + 1
        );
      } else {
        sprintf_P(
          text,
          PSTR("Channel %s\nNone"),
          switchNames[sw]
        );
      }
      break;
    case SCREEN_BATTARY_LOW:
      sprintf_P(
        text,
        PSTR("Bat low\n%d.%03dV"),
        settings->values.battaryLowMV /1000,
        settings->values.battaryLowMV % 1000
      );
      break;
    case SCREEN_SAVE_FOR_NOLINK:
      sprintf_P(
        text,
        PSTR("Save for\nno link?")
      );
      break;
    case SCREEN_SAVE:
      sprintf_P(
        text,
        PSTR("Save?")
      );
      break;
#ifndef FLAT_MENU
    case SCREEN_GROUP_CONTROLS:
      sprintf_P(
        text,
        PSTR("Controls>")
      );
      break;
    case SCREEN_GROUP_MAPPING:
      sprintf_P(
        text,
        PSTR("Mapping>")
      );
      break;
    case SCREEN_GROUP_PEER:
      sprintf_P(
        text,
        PSTR("Peer>")
      );
      break;
    case SCREEN_MENU_UP:
      sprintf_P(
        text,
        PSTR("<")
      );
      break;
#endif
  }

#if defined(WITH_ADAFRUIT_SSD1306)
  display.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, BLACK);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
#elif defined(WITH_SSD1306_ASCII)
  display.set2X();
  display.clear();
  display.print(text);
#endif
}

void ControlPannel::moveMenuTop() {
#ifdef FLAT_MENU
  currentScreen = FIRST_SCREEN;
#else
  menuStack[0] = mainMenu;
  currentMenu = &menuStack[0];
  currentMenuItem = *currentMenu;
#endif
}

void ControlPannel::moveMenuForward() {
#ifdef FLAT_MENU
  if (currentScreen < LAST_SCREEN) {
    currentScreen += 1;
  } else {
    currentScreen = FIRST_SCREEN;
  }
#else
  currentMenuItem++;
  if (*currentMenuItem == SCREEN_NULL) {
    currentMenuItem = *currentMenu;
  }
#endif
}

#ifndef FLAT_MENU
void ControlPannel::moveMenuDown() {
  Screen *newMenu = NULL;

  for (int i = 0; i < sizeof(subMenu) / sizeof(subMenu[0]); i++) {
      if (subMenu[i].screen == *currentMenuItem) {
        newMenu = subMenu[i].subMenu;
        break;
      }
  }

  if (newMenu == NULL) return;
  currentMenu++;
  *currentMenu = newMenu;
  currentMenuItem = newMenu;
}

void ControlPannel::moveMenuUp() {
  Screen prevMenu = SCREEN_NULL;

  if (currentMenu == &menuStack[0]) return;

  for (int i = 0; i < sizeof(subMenu) / sizeof(subMenu[0]); i++) {
    if (subMenu[i].subMenu == *currentMenu) {
      prevMenu = subMenu[i].screen;
      break;
    }
  }

  currentMenu--;
  currentMenuItem = *currentMenu;
  if (prevMenu != SCREEN_NULL) {
    while (
        *currentMenuItem != SCREEN_NULL
        && *currentMenuItem != prevMenu
    ) currentMenuItem++;
  }
}
#endif

void ControlPannel::handle() {
  int settingsValueChange = 0;
  Axis axis;
  Switch sw;
  Screen prevScreen = currentScreen;
  unsigned long now = millis();

  buttons.handle();

  if (settingsButton.resetClicked()) {
    if (settingsLongPress) {
      settingsLongPress = false;
    } else {
      moveMenuForward();
    }
  }

  if (settingsButton.isHeld() && currentScreen != SCREEN_BLANK) {
    moveMenuTop();
    settingsLongPress = true;
  }

  if (currentScreen != prevScreen) {
    PRINT(F("Screen: "));
    PRINTLN(currentScreen);
    redrawScreen();
  }

  if (settingsPlusButton.resetClicked()) {
    settingsValueChange = 1;
  }
  if (settingsMinusButton.resetClicked()) {
    settingsValueChange = -1;
  }

  if (settingsValueChange != 0) {
    switch (currentScreen) {
      case SCREEN_BLANK:
        radioControl->sendCommand(
          (settingsValueChange > 0) ?
          COMMAND_USER_COMMAND1 : COMMAND_USER_COMMAND2
        );
        buzzer->beep(BEEP_LOW_HZ, 250, 0, 1);
        break;
      case SCREEN_PROFILE:
        addWithConstrain(
          settings->currentProfile, settingsValueChange, 0, NUM_PROFILES - 1
        );
        settings->loadProfile();
        radioControl->radio->setRFChannel(settings->values.rfChannel);
        break;
      case SCREEN_RF_CHANNEL:
        addWithConstrain(
          settings->values.rfChannel, settingsValueChange, 0, 125
        );
        radioControl->sendRFChannel(settings->values.rfChannel);
        radioControl->radio->setRFChannel(settings->values.rfChannel);
        break;
      case SCREEN_AUTO_CENTER:
        if (settingsValueChange > 0) {
          buzzer->beep(BEEP_LOW_HZ, 1000, 0, 1);
          buzzer->handle();
          controls->setJoystickCenter();
          buzzer->noBeep();
          buzzer->handle();
        }
        break;
      case SCREEN_DUAL_RATE_A_X:
      case SCREEN_DUAL_RATE_A_Y:
      case SCREEN_DUAL_RATE_B_X:
      case SCREEN_DUAL_RATE_B_Y:
        axis = currentScreen - SCREEN_DUAL_RATE_A_X;
        addWithConstrain(
          settings->values.axes[axis].dualRate,
          settingsValueChange * 10,
          DUAL_RATE_MIN,
          DUAL_RATE_MAX
        );
        break;
      case SCREEN_TRIMMING_A_X:
      case SCREEN_TRIMMING_A_Y:
      case SCREEN_TRIMMING_B_X:
      case SCREEN_TRIMMING_B_Y:
        axis = currentScreen - SCREEN_TRIMMING_A_X;
        addWithConstrain(
          settings->values.axes[axis].trimming,
          settingsValueChange * 5,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_INVERT_A_X:
      case SCREEN_INVERT_A_Y:
      case SCREEN_INVERT_B_X:
      case SCREEN_INVERT_B_Y:
        axis = currentScreen - SCREEN_INVERT_A_X;
        settings->values.axes[axis].joyInvert = settingsValueChange > 0;
        break;
      case SCREEN_LOW_SWITCH_1:
      case SCREEN_LOW_SWITCH_2:
        sw = currentScreen - SCREEN_LOW_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].low,
          settingsValueChange * 50,
          SWITCH_MIN,
          SWITCH_MAX
        );
        break;
      case SCREEN_HIGH_SWITCH_1:
      case SCREEN_HIGH_SWITCH_2:
        sw = currentScreen - SCREEN_HIGH_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].high,
          settingsValueChange * 50,
          SWITCH_MIN,
          SWITCH_MAX
        );
        break;
      case SCREEN_CHANNEL_A_X:
      case SCREEN_CHANNEL_A_Y:
      case SCREEN_CHANNEL_B_X:
      case SCREEN_CHANNEL_B_Y:
        axis = currentScreen - SCREEN_CHANNEL_A_X;
        addWithConstrain(
          settings->values.axes[axis].channel,
          settingsValueChange,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_CHANNEL_SWITCH_1:
      case SCREEN_CHANNEL_SWITCH_2:
        sw = currentScreen - SCREEN_CHANNEL_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].channel,
          settingsValueChange,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_BATTARY_LOW:
        addWithConstrain(
          settings->values.battaryLowMV,
          settingsValueChange * 100,
          100,
          20000
        );
        break;
      case SCREEN_SAVE_FOR_NOLINK:
        if (settingsValueChange > 0) {
          radioControl->sendCommand(COMMAND_SAVE_FOR_NOLINK);
          buzzer->beep(BEEP_LOW_HZ, 250, 0, 1);
        }
        break;
      case SCREEN_SAVE:
        moveMenuTop();
        if (settingsValueChange > 0) {
          settings->saveProfile();
          buzzer->beep(BEEP_LOW_HZ, 500, 0, 1);
        }
        break;
#ifndef FLAT_MENU
      case SCREEN_GROUP_CONTROLS:
      case SCREEN_GROUP_MAPPING:
      case SCREEN_GROUP_PEER:
        if (settingsValueChange > 0) moveMenuDown();
        break;
      case SCREEN_MENU_UP:
        moveMenuUp();
        break;
#endif
    }
    redrawScreen();
  }

  if (now - battaryUpdateTime > 5000) {
    battaryUpdateTime = now;
    thisBattaryMV = getBattaryVoltage();
    PRINT(F("This device battary (mV): "));
    PRINTLN(thisBattaryMV);
    redrawScreen();
  }

  if (radioControl->radio->receive(&telemetry)) {
    PRINT(F("Peer device battary (mV): "));
    PRINTLN(telemetry.battaryMV);
    if (
      telemetry.battaryMV > 0 && telemetry.battaryMV < settings->values.battaryLowMV
    ) {
      buzzer->beep(BEEP_LOW_HZ, 200, 100, 3);
    }
  }
}

// vim:ai:sw=2:et
