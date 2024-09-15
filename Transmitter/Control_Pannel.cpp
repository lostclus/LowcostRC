#include <LowcostRC_Console.h>
#include "Battary.h"
#include "Settings.h"
#include "Control_Pannel.h"

#define DUAL_RATE_MIN    10
#define DUAL_RATE_MAX  1500
#define TRIMMING_MIN   -1500
#define TRIMMING_MAX   1500
#define SWITCH_MIN        0
#define SWITCH_MAX     3000

#define FLAG_SETTINGS_LONG_PRESS 0
#define FLAG_IS_PAIRING          1
#define FLAG_CURSOR_MOVE         2
#define FLAG_CURSOR_BLINK        3

#ifndef FLAT_MENU
const Screen radioMenu[] = {
  SCREEN_BIND_PEER,
  SCREEN_PEER_ADDR,
  SCREEN_RF_CHANNEL,
  SCREEN_MENU_UP,
  SCREEN_NULL
};

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
  SCREEN_SAVE_FAILSAFE,
  SCREEN_MENU_UP,
  SCREEN_NULL
};

const Screen mainMenu[] = {
  SCREEN_BLANK,
  SCREEN_DISPLAY,
  SCREEN_PROFILE,
  SCREEN_PROFILE_NAME,
  SCREEN_GROUP_RADIO,
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
  {SCREEN_GROUP_RADIO, radioMenu},
  {SCREEN_GROUP_CONTROLS, controlsMenu},
  {SCREEN_GROUP_MAPPING, mappingMenu},
  {SCREEN_GROUP_PEER, peerMenu},
};
#endif

#define addWithConstrain(value, delta, lo, hi) value = constrain((long)value + (delta), lo, hi)

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
  , screenButton(KEY_SCREEN_PIN)
  , plusButton(KEY_PLUS_PIN)
  , minusButton(KEY_MINUS_PIN)
{
  moveMenuTop();
}

void ControlPannel::begin() {
  screenButton.begin();
  screenButton.setDebounceTime(20);
  plusButton.begin();
  plusButton.setDebounceTime(20);
  minusButton.begin();
  minusButton.setDebounceTime(20);

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

  radioControl->radio->setPeer(&settings->values.peer);
  radioControl->radio->setRFChannel(settings->values.rfChannel);
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
    case SCREEN_DISPLAY:
      sprintf_P(
        text,
        PSTR("=== %d. %s ===\n"
             "TxBt: %d.%02dV\n"
             "RxBt: %d.%02dV\n"
             "LQI: %d%%"),
        settings->currentProfile + 1,
        settings->values.profileName,
        thisBattaryMV / 1000,
        (thisBattaryMV % 1000) / 10,
        radioControl->telemetry.battaryMV / 1000,
        (radioControl->telemetry.battaryMV % 1000) / 10,
        radioControl->linkQuality
      );
      break;
    case SCREEN_PROFILE:
      sprintf_P(
        text,
        PSTR("Profile\n%d\n%s"),
        settings->currentProfile + 1,
        settings->values.profileName
      );
      break;
    case SCREEN_PROFILE_NAME:
      sprintf_P(
        text,
        PSTR("Name\n%s"),
        settings->values.profileName
      );
      if (bitRead(flags, FLAG_CURSOR_BLINK))
        text[cursor + 5] = '_';
      break;
    case SCREEN_BIND_PEER:
      if (bitRead(flags, FLAG_IS_PAIRING)) {
        sprintf_P(
          text,
          PSTR("Pairing...")
        );
      } else {
        sprintf_P(
          text,
          PSTR("Paired\n%s"),
          radioControl->radio->isPaired() ? yStr : nStr
        );
      }
      break;
    case SCREEN_PEER_ADDR:
      sprintf_P(
        text,
        PSTR("Peer\n%02x:%02x:%02x\n%02x:%02x:%02x"),
        settings->values.peer.address[0],
        settings->values.peer.address[1],
        settings->values.peer.address[2],
        settings->values.peer.address[3],
        settings->values.peer.address[4],
        settings->values.peer.address[5]
      );
      if (bitRead(flags, FLAG_CURSOR_BLINK)) {
        text[cursor * 3 + 5] = text[cursor * 3 + 6] = '_';
      }
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
          PSTR("Ch %s\nNone"),
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
    case SCREEN_SAVE_FAILSAFE:
      sprintf_P(
        text,
        PSTR("Save failsafe?")
      );
      break;
    case SCREEN_SAVE:
      sprintf_P(
        text,
        PSTR("Save?")
      );
      break;
#ifndef FLAT_MENU
    case SCREEN_GROUP_RADIO:
      sprintf_P(
        text,
        PSTR("Radio>")
      );
      break;
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
  if (currentScreen == SCREEN_DISPLAY) {
    display.setTextSize(1);
  } else {
    display.setTextSize(2);
  }
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
#elif defined(WITH_SSD1306_ASCII)
  if (currentScreen == SCREEN_DISPLAY) {
    display.set1X();
  } else {
    display.set2X();
  }
  display.clear();
  display.print(text);
#endif
  redrawTime = millis();
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
  int change = 0;
  Axis axis;
  Switch sw;
  Screen prevScreen = currentScreen;
  bool needsRedraw = false;
  unsigned long now = millis();

  screenButton.handle();
  plusButton.handle();
  minusButton.handle();

  if (screenButton.resetClicked()) {
    if (bitRead(flags, FLAG_SETTINGS_LONG_PRESS)) {
      bitClear(flags, FLAG_SETTINGS_LONG_PRESS);
    } else {
      moveMenuForward();
    }
  }

  if (screenButton.isHeld() && currentScreen != SCREEN_BLANK) {
    moveMenuTop();
    bitSet(flags, FLAG_SETTINGS_LONG_PRESS);
  }

  if (currentScreen != prevScreen) {
    PRINT(F("Screen: "));
    PRINTLN(currentScreen);
    redrawScreen();
  }

  if (
    plusButton.resetClicked()
    || (plusButton.isHeld() && now - settingsChangeTime > 200)
  ) {
    change = 1;
    settingsChangeTime = now;
  }
  if (
    minusButton.resetClicked()
    || (minusButton.isHeld() && now - settingsChangeTime > 200)
  ) {
    change = -1;
    settingsChangeTime = now;
  }

  if (change) {
    switch (currentScreen) {
      case SCREEN_BLANK:
        radioControl->sendCommand(
          (change > 0) ?
          COMMAND_USER_COMMAND1 : COMMAND_USER_COMMAND2
        );
        buzzer->beep(BEEP_LOW_HZ, 250, 0, 1);
        break;
      case SCREEN_PROFILE:
        addWithConstrain(
          settings->currentProfile, change, 0, NUM_PROFILES - 1
        );
        settings->loadProfile();
        radioControl->radio->setPeer(&settings->values.peer);
        radioControl->radio->setRFChannel(settings->values.rfChannel);
        break;
      case SCREEN_PROFILE_NAME:
        if (change) {
          addWithConstrain(
            settings->values.profileName[cursor], change, 0x20, 0x7f
          );
          bitSet(flags, FLAG_CURSOR_MOVE);
        }
        break;
      case SCREEN_BIND_PEER:
        if (change > 0) {
          bitSet(flags, FLAG_IS_PAIRING);
          redrawScreen();
          if (radioControl->radio->pair()) {
            memcpy(
              settings->values.peer.address,
              radioControl->radio->peer.address,
              ADDRESS_LENGTH
            );
            buzzer->beep(BEEP_LOW_HZ, 30, 30, 1);
            settings->values.rfChannel = DEFAULT_RF_CHANNEL;
          } else {
            buzzer->beep(BEEP_HIGH_HZ, 5, 30, 5);
          }
          bitClear(flags, FLAG_IS_PAIRING);
          needsRedraw = true;
        } else {
          radioControl->radio->unpair();
          memcpy(
            settings->values.peer.address,
            radioControl->radio->peer.address,
            ADDRESS_LENGTH
          );
          settings->values.rfChannel = DEFAULT_RF_CHANNEL;
        }
        break;
      case SCREEN_PEER_ADDR:
        if (change) {
          addWithConstrain(settings->values.peer.address[cursor], change, 0x00, 0xff);
          radioControl->radio->setPeer(&settings->values.peer);
          radioControl->radio->setRFChannel(settings->values.rfChannel);
          bitSet(flags, FLAG_CURSOR_MOVE);
        }
        break;
      case SCREEN_RF_CHANNEL:
        addWithConstrain(
          settings->values.rfChannel, change, 0, 125
        );
        radioControl->sendRFChannel(settings->values.rfChannel);
        radioControl->radio->setRFChannel(settings->values.rfChannel);
        break;
      case SCREEN_AUTO_CENTER:
        if (change > 0) {
          buzzer->beep(BEEP_LOW_HZ, 500, 0, 1);
          buzzer->handle();
          controls->setJoystickCenter();
        }
        break;
      case SCREEN_DUAL_RATE_A_X:
      case SCREEN_DUAL_RATE_A_Y:
      case SCREEN_DUAL_RATE_B_X:
      case SCREEN_DUAL_RATE_B_Y:
        axis = currentScreen - SCREEN_DUAL_RATE_A_X;
        addWithConstrain(
          settings->values.axes[axis].dualRate,
          change * 10,
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
          change * 5,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_INVERT_A_X:
      case SCREEN_INVERT_A_Y:
      case SCREEN_INVERT_B_X:
      case SCREEN_INVERT_B_Y:
        axis = currentScreen - SCREEN_INVERT_A_X;
        settings->values.axes[axis].joyInvert = change > 0;
        break;
      case SCREEN_LOW_SWITCH_1:
      case SCREEN_LOW_SWITCH_2:
        sw = currentScreen - SCREEN_LOW_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].low,
          change * 50,
          SWITCH_MIN,
          SWITCH_MAX
        );
        break;
      case SCREEN_HIGH_SWITCH_1:
      case SCREEN_HIGH_SWITCH_2:
        sw = currentScreen - SCREEN_HIGH_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].high,
          change * 50,
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
          change,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_CHANNEL_SWITCH_1:
      case SCREEN_CHANNEL_SWITCH_2:
        sw = currentScreen - SCREEN_CHANNEL_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].channel,
          change,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_BATTARY_LOW:
        addWithConstrain(
          settings->values.battaryLowMV,
          change * 100,
          100,
          20000
        );
        break;
      case SCREEN_SAVE_FAILSAFE:
        if (change > 0) {
          radioControl->sendCommand(COMMAND_SAVE_FAILSAFE);
          buzzer->beep(BEEP_LOW_HZ, 250, 0, 1);
        }
        break;
      case SCREEN_SAVE:
        moveMenuTop();
        if (change > 0) {
          settings->saveProfile();
          buzzer->beep(BEEP_LOW_HZ, 500, 0, 1);
        }
        break;
#ifndef FLAT_MENU
      case SCREEN_GROUP_RADIO:
      case SCREEN_GROUP_CONTROLS:
      case SCREEN_GROUP_MAPPING:
      case SCREEN_GROUP_PEER:
        if (change > 0) moveMenuDown();
        break;
      case SCREEN_MENU_UP:
        moveMenuUp();
        break;
#endif
    }
    needsRedraw = true;
  }

  switch (currentScreen) {
    case SCREEN_PROFILE_NAME:
    case SCREEN_PEER_ADDR:
      if (bitRead(flags, FLAG_CURSOR_MOVE) && now - settingsChangeTime > 5000) {
        bitClear(flags, FLAG_CURSOR_MOVE);
        cursor++;
        if (
          (
            currentScreen == SCREEN_PROFILE_NAME
            && cursor >= sizeof(settings->values.profileName)
          )
          || (
            currentScreen == SCREEN_PEER_ADDR
            && cursor >= sizeof(settings->values.peer.address)
          )
        ) {
          cursor = 0;
        }
      }
      if ((now / 100) % 10 < 4) {
        if (!bitRead(flags, FLAG_CURSOR_BLINK)) {
          bitSet(flags, FLAG_CURSOR_BLINK);
          needsRedraw = true;
        }
      } else {
        if (bitRead(flags, FLAG_CURSOR_BLINK)) {
          bitClear(flags, FLAG_CURSOR_BLINK);
          needsRedraw = true;
        }
      }
      break;
    default:
      if (cursor != 0) cursor = 0;
  }

  if (now - battaryUpdateTime > BATTARY_MONITOR_INTERVAL) {
    battaryUpdateTime = now;
    thisBattaryMV = getBattaryVoltage();
    PRINT(F("This device battary (mV): "));
    PRINTLN(thisBattaryMV);
    if (
      radioControl->telemetry.battaryMV > 0
      && radioControl->telemetry.battaryMV < settings->values.battaryLowMV
    ) {
      buzzer->beep(BEEP_LOW_HZ, 200, 100, 3);
    }
  }

  if (
      now - redrawTime > SCREEN_DISPLAY_REDRAW_INTERVAL
      && currentScreen == SCREEN_DISPLAY
  )
    needsRedraw = true;

  if (needsRedraw) redrawScreen();
}

// vim:ai:sw=2:et
