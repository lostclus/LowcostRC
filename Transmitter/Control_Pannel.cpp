#include <LowcostRC_Console.h>
#include "Battary.h"
#include "Control_Pannel.h"

const int DUAL_RATE_MIN = 10,
          DUAL_RATE_MAX = 1500,
          TRIMMING_MIN = -1500,
          TRIMMING_MAX = 1500,
          SWITCH_MIN = 0,
          SWITCH_MAX = 3000;

#define addWithConstrain(value, delta, lo, hi) value = constrain(value + (delta), lo, hi)

ControlPannel::ControlPannel(
  Settings *settings, Buzzer *buzzer, RadioControl *radioControl, Controls *controls
)
  : settings(settings)
  , buzzer(buzzer)
  , radioControl(radioControl)
  , controls(controls)
  , display(128, 64, &Wire, -1)
  , settingsButton(SETTINGS_PIN)
  , settingsPlusButton(SETTINGS_PLUS_PIN)
  , settingsMinusButton(SETTINGS_MINUS_PIN)
  , buttonsArray({&settingsButton, &settingsPlusButton, &settingsMinusButton})
  , buttons(buttonsArray)
{
}

void ControlPannel::begin() {
  buttons.begin();
  settingsButton.setDebounceTime(20);
  settingsPlusButton.setDebounceTime(20);
  settingsMinusButton.setDebounceTime(20);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
}

void ControlPannel::redrawScreen() {
  char text[50] = "",
       yStr[] = "y",
       nStr[] = "n",
       axisNames[][3] = {"AX", "AY", "BX", "BY"},
       switchNames[][4] = {"SW1", "SW2"};
  Axis axis;
  Switch sw;

  display.fillRect(0, 0, 128, 64, BLACK);

  switch (screenNum) {
    case NO_SCREEN:
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
      axis = screenNum - SCREEN_DUAL_RATE_A_X;
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
      axis = screenNum - SCREEN_TRIMMING_A_X;
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
      axis = screenNum - SCREEN_INVERT_A_X;
      sprintf_P(
        text,
        PSTR("Invert %s\n%s"),
        axisNames[axis],
        settings->values.axes[axis].joyInvert ? yStr : nStr
      );
      break;
    case SCREEN_CHANNEL_A_X:
    case SCREEN_CHANNEL_A_Y:
    case SCREEN_CHANNEL_B_X:
    case SCREEN_CHANNEL_B_Y:
      axis = screenNum - SCREEN_CHANNEL_A_X;
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
    case SCREEN_LOW_SWITCH_1:
    case SCREEN_LOW_SWITCH_2:
      sw = screenNum - SCREEN_LOW_SWITCH_1;
      sprintf_P(
        text,
        PSTR("Low %s\n%d"),
        switchNames[sw],
        settings->values.switches[sw].low
      );
      break;
    case SCREEN_HIGH_SWITCH_1:
    case SCREEN_HIGH_SWITCH_2:
      sw = screenNum - SCREEN_HIGH_SWITCH_1;
      sprintf_P(
        text,
        PSTR("High %s\n%d"),
        switchNames[sw],
        settings->values.switches[sw].high
      );
      break;
    case SCREEN_CHANNEL_SWITCH_1:
    case SCREEN_CHANNEL_SWITCH_2:
      sw = screenNum - SCREEN_CHANNEL_SWITCH_1;
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
  }

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}

void ControlPannel::handle() {
  int settingsValueChange = 0;
  Screen prevScreenNum = screenNum;
  Axis axis;
  Switch sw;
  unsigned long now = millis();

  buttons.handle();

  if (settingsButton.resetClicked()) {
    if (settingsLongPress) {
      settingsLongPress = false;
    } else {
      screenNum = screenNum + ((Screen)1);
      if (screenNum > LAST_SCREEN)
        screenNum = FIRST_SCREEN;
      PRINT(F("Screen: "));
      PRINTLN(screenNum);
      redrawScreen();
    }
  }
  if (settingsButton.isHeld() && screenNum != NO_SCREEN) {
    screenNum = NO_SCREEN;
    PRINT(F("Screen: "));
    PRINTLN(screenNum);
    redrawScreen();
    settingsLongPress = true;
  }

  if (settingsPlusButton.resetClicked()) {
    settingsValueChange = 1;
  }
  if (settingsMinusButton.resetClicked()) {
    settingsValueChange = -1;
  }

  if (settingsValueChange != 0) {
    switch (screenNum) {
      case NO_SCREEN:
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
        axis = screenNum - SCREEN_DUAL_RATE_A_X;
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
        axis = screenNum - SCREEN_TRIMMING_A_X;
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
        axis = screenNum - SCREEN_INVERT_A_X;
        settings->values.axes[axis].joyInvert = settingsValueChange > 0;
        break;
      case SCREEN_CHANNEL_A_X:
      case SCREEN_CHANNEL_A_Y:
      case SCREEN_CHANNEL_B_X:
      case SCREEN_CHANNEL_B_Y:
        axis = screenNum - SCREEN_CHANNEL_A_X;
        addWithConstrain(
          settings->values.axes[axis].channel,
          settingsValueChange,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_LOW_SWITCH_1:
      case SCREEN_LOW_SWITCH_2:
        sw = screenNum - SCREEN_LOW_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].low,
          settingsValueChange * 50,
          SWITCH_MIN,
          SWITCH_MAX
        );
        break;
      case SCREEN_HIGH_SWITCH_1:
      case SCREEN_HIGH_SWITCH_2:
        sw = screenNum - SCREEN_HIGH_SWITCH_1;
        addWithConstrain(
          settings->values.switches[sw].high,
          settingsValueChange * 50,
          SWITCH_MIN,
          SWITCH_MAX
        );
        break;
      case SCREEN_CHANNEL_SWITCH_1:
      case SCREEN_CHANNEL_SWITCH_2:
        sw = screenNum - SCREEN_CHANNEL_SWITCH_1;
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
        screenNum = NO_SCREEN;
        if (settingsValueChange > 0) {
          settings->saveProfile();
          buzzer->beep(BEEP_LOW_HZ, 500, 0, 1);
        }
        break;
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
