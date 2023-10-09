#include <string.h>
#include <EEPROM.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AbleButtons.h>

#include "protocol.h"

#undef WITH_CONSOLE
#define WITH_CONSOLE

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define BEEP_PIN 3
#define BEEP_FREQ 1320
#define SETTINGS_PIN 6
#define SETTINGS_PLUS_PIN 7
#define SETTINGS_MINUS_PIN 8

#define VOLT_METER_PIN A6
#define VOLT_METER_R1 10000L
#define VOLT_METER_R2 10000L

#define SETTINGS_MAGICK 0x5556
#define PROFILES_ADDR 0
#define NUM_PROFILES 5

enum Axis {
  AXIS_A_X,
  AXIS_A_Y,
  AXIS_B_X,
  AXIS_B_Y,
  AXES_COUNT,
};

int joystickPins[] = {A1, A0, A3, A2};

struct AxisSettings {
  int joyCenter,
      joyThreshold;
  bool joyInvert;
  int dualRate,
      trimming;
  ChannelN channel;
};

struct Settings {
  int magick;
  int pipeAddressN;
  AxisSettings axes[AXES_COUNT];
};

const int DEFAULT_PIPE_ADDRESS_N = 0,
          DEFAULT_JOY_CENTER = 512,
          DEFAULT_JOY_THRESHOLD = 1;
const bool DEFAULT_JOY_INVERT = false;
const int CENTER_PULSE = 1500,
          DEFAULT_DUAL_RATE = 900,
          DEFAULT_TRIMMING = 0,
          DUAL_RATE_MIN = 10,
          DUAL_RATE_MAX = 1500,
          TRIMMING_MIN = -1500,
          TRIMMING_MAX = 1500;

const Settings defaultSettings PROGMEM = {
  SETTINGS_MAGICK,
  DEFAULT_PIPE_ADDRESS_N,
  {
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL1
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL2
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL3
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL4
    }
  }
};

int currentProfile = 0;
Settings settings;
#define SETTINGS_SIZE sizeof(settings)

enum Screen {
  NO_SCREEN,
  SCREEN_BATTARY,
  SCREEN_PROFILE,
  SCREEN_PIPE_ADDRESS,
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
  SCREEN_SAVE_FOR_NOLINK,
  SCREEN_SAVE,
  FIRST_SCREEN = NO_SCREEN,
  LAST_SCREEN = SCREEN_SAVE,
};

unsigned int thisBattaryMV = 0;

unsigned long requestSendTime = 0,
              errorTime = 0,
              beepTime = 0,
              battaryUpdateTime = 0;
bool statusRadioSuccess = false,
     statusRadioFailure = false,
     beepState = false,
     settingsLongPress = false;
int beepDuration = 0,
    beepPause = 0,
    beepCount = 0;
Screen screenNum = NO_SCREEN;

struct StatusPacket status;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

Button settingsButton(SETTINGS_PIN),
       settingsPlusButton(SETTINGS_PLUS_PIN),
       settingsMinusButton(SETTINGS_MINUS_PIN);

Button *buttonsArray[] = {
  &settingsButton,
  &settingsPlusButton,
  &settingsMinusButton
};
ButtonList buttons(buttonsArray);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

#define addWithConstrain(value, delta, lo, hi) value = constrain(value + (delta), lo, hi)

bool loadProfile();
void saveProfile();
char *formatPipeAddress(int pipeAddressN);
void setPipeAddress(int pipeAddressN);
void sendPipeAddress(unsigned long now, int pipeAddressN);
void sendCommand(unsigned long now);
void controlLoop(unsigned long now);
int readAxis(Axis axis);
void setJoystickCenter();
void updateBattaryVoltage();
void controlBeep(unsigned long now);
void redrawScreen();
void controlScreen(unsigned long now);

void setup(void)
{
  bool needsSetJoystickCenter = false;
  char *pipeAddress;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));

  for (int axis = 0; axis < AXES_COUNT; axis++) {
    pinMode(joystickPins[axis], INPUT);
  }

  buttons.begin();
  settingsButton.setDebounceTime(20);
  settingsPlusButton.setDebounceTime(20);
  settingsMinusButton.setDebounceTime(20);

  pinMode(BEEP_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  if (!loadProfile())
    needsSetJoystickCenter = true;

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN("Radio init: FAILURE");
  }
  radio.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  radio.setPayloadSize(PACKET_SIZE);
  radio.enableAckPayload();

  setPipeAddress(settings.pipeAddressN);

  if (needsSetJoystickCenter) {
    setJoystickCenter();
  }
}

void loop(void) {
  unsigned long now = millis();

  controlLoop(now);

  if (errorTime > 0 && now - errorTime > 250) {
    statusRadioFailure = false;
    errorTime = 0;
  }
  if (requestSendTime > 0 && now - requestSendTime > 250) {
    statusRadioSuccess = false;
  }

  if (now - battaryUpdateTime > 5000) {
    battaryUpdateTime = now;
    updateBattaryVoltage();
    PRINT(F("battaryMV: "));
    PRINTLN(thisBattaryMV);
    redrawScreen();
  }

  controlBeep(now);
  controlScreen(now);
}

bool loadProfile() {
  PRINT(F("Reading profile #"));
  PRINT(currentProfile);
  PRINTLN(F(" from flash ROM..."));
  EEPROM.get(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, settings);

  if (settings.magick != SETTINGS_MAGICK) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&settings, &defaultSettings, SETTINGS_SIZE);
    return false;
  } 

  PRINTLN(F("Using stored settings in flash ROM"));
  return true;
}

void saveProfile() {
  EEPROM.put(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, settings);
}

char *formatPipeAddress(int pipeAddressN) {
  static char addr[7];
  sprintf_P(addr, PSTR("lcrc%02d"), pipeAddressN);
  return addr;
}

void setPipeAddress(int pipeAddressN) {
  char *pipeAddress = formatPipeAddress(settings.pipeAddressN);
  radio.openWritingPipe((byte*)pipeAddress);
  PRINT(F("Radio pipe address: "));
  PRINTLN(pipeAddress);
}

void sendPipeAddress(unsigned long now, int pipeAddressN) {
  char *pipeAddress = formatPipeAddress(settings.pipeAddressN);
  union RequestPacket rp;
  rp.address.packetType = PACKET_TYPE_SET_PIPE_ADDRESS;
  strcpy(rp.address.pipeAddress, pipeAddress);
  sendRequest(now, &rp);
}

void sendCommand(unsigned long now, Command command) {
  union RequestPacket rp;
  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = command;
  sendRequest(now, &rp);
}

void controlLoop(unsigned long now) {
  union RequestPacket rp;
  bool isChanged = false;
  static int prevChannels[NUM_CHANNELS];

  rp.control.packetType = PACKET_TYPE_CONTROL;

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    rp.control.channels[channel] = 0;
  for (int axis = 0; axis < AXES_COUNT; axis++) {
    ChannelN channel = settings.axes[axis].channel;
    if (channel != NO_CHANNEL) {
      rp.control.channels[channel] = readAxis(axis);
    }
  }

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    isChanged = isChanged || rp.control.channels[channel] != prevChannels[channel];

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    prevChannels[channel] = rp.control.channels[channel];

  if (
    isChanged
    || (requestSendTime > 0 && now - requestSendTime > 1000)
    || (errorTime > 0 && now - errorTime < 200)
  ) {
    PRINT(F("ch1: "));
    PRINT(rp.control.channels[CHANNEL1]);
    PRINT(F("; ch2: "));
    PRINT(rp.control.channels[CHANNEL2]);
    PRINT(F("; ch3: "));
    PRINT(rp.control.channels[CHANNEL3]);
    PRINT(F("; ch4: "));
    PRINTLN(rp.control.channels[CHANNEL4]);

    sendRequest(now, &rp);
  }

  if (radio.isAckPayloadAvailable()) {
    radio.read(&status, sizeof(status));
    if (status.packetType == PACKET_TYPE_STATUS) {
      PRINT(F("battaryMV: "));
      PRINTLN(status.battaryMV);
      if (status.battaryMV < 3400) {
        beepCount = 3;
        beepDuration = 200;
        beepPause = 100;
      }
    }
  }
}

void sendRequest(unsigned long now, union RequestPacket *packet) {
  static bool prevStatusRadioSuccess = false,
              prevStatusRadioFailure = false;
  bool radioOK, isStatusChanged;

  PRINT(F("Sending packet type: "));
  PRINT(packet->generic.packetType);
  PRINT(F("; size: "));
  PRINTLN(sizeof(*packet));

  radioOK = radio.write(packet, sizeof(*packet));
  if (radioOK) {
    requestSendTime = now;
    errorTime = 0;
    statusRadioSuccess = true;
    statusRadioFailure = false;
  } else {
    if (errorTime == 0) errorTime = now;
    requestSendTime = 0;
    statusRadioFailure = true;
    statusRadioSuccess = false;
    
    beepCount = 1;
    beepDuration = 5;
    beepPause = 5;
  }

  isStatusChanged = (
    statusRadioSuccess != prevStatusRadioSuccess
    || statusRadioFailure != prevStatusRadioFailure
  );
  prevStatusRadioSuccess = statusRadioSuccess;
  prevStatusRadioFailure = statusRadioFailure;

  if (isStatusChanged && statusRadioSuccess) {
    beepCount = 1;
    beepDuration = 50;
    beepPause = 50;
  }
}

int mapAxis(
  int joyValue,
  int joyCenter,
  int joyThreshold,
  bool joyInvert,
  int dualRate,
  int trimming
) {
  int centerPulse = CENTER_PULSE + trimming,
      minPulse = centerPulse - dualRate,
      maxPulse = centerPulse + dualRate,
      pulse = centerPulse;

  if (joyInvert) {
    joyValue = 1023 - joyValue;
    joyCenter = 1023 - joyCenter;
  }

  if (joyValue >= joyCenter - joyThreshold && joyValue <= joyCenter + joyThreshold)
    pulse = centerPulse;
  else if (joyValue < joyCenter)
    pulse = map(joyValue, 0, joyCenter - joyThreshold, minPulse, centerPulse);
  else if (joyValue > joyCenter)
    pulse = map(joyValue, joyCenter + joyThreshold, 1023, centerPulse, maxPulse);

  return constrain(pulse, 0, 5000);
}

int readAxis(Axis axis) {
  int joyValue = analogRead(joystickPins[axis]);
  return mapAxis(
    joyValue,
    settings.axes[axis].joyCenter,
    settings.axes[axis].joyThreshold,
    settings.axes[axis].joyInvert,
    settings.axes[axis].dualRate,
    settings.axes[axis].trimming
  );
}

void setJoystickCenter() {
  int value[AXES_COUNT] = {0, 0, 0, 0},
      count = 5;

  PRINT(F("Setting joystick center..."));

  for (int i = 0; i < count; i++) {
    for (int axis = 0; axis < AXES_COUNT; axis++) {
      value[axis] += analogRead(joystickPins[axis]);
    }
    delay(100);
  }

  for (int axis = 0; axis < AXES_COUNT; axis++) {
    settings.axes[axis].joyCenter = value[axis] / count;
  }

  PRINTLN(F("DONE"));
}

unsigned long vHist[5] = {0, 0, 0, 0, 0};
byte vHistPos = 0;

void updateBattaryVoltage() {
  byte i, count = 0;
  unsigned long vcc = 0, vpin, vsum = 0;
   
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
      ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
      ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
      ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
      // works on an Arduino 168 or 328
      ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  delay(3); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  // 1.1 * 1023 * 1000 = 1125300
  vcc = 1125300L / ((unsigned long)((high<<8) | low));
  vpin = analogRead(VOLT_METER_PIN);

  vHist[vHistPos] = vpin * vcc;
  vHistPos = (vHistPos + 1) % (sizeof(vHist) / sizeof(vHist[0]));

  for (i = 0; i < sizeof(vHist) / sizeof(vHist[0]); i++) {
    if (vHist[i] > 0) {
      vsum += vHist[i];
      count += 1;
    }
  }

  thisBattaryMV = (vsum / count) / 1024 
    * (1000L / (VOLT_METER_R2 * 1000L / (VOLT_METER_R1 + VOLT_METER_R2)));
}

void controlBeep(unsigned long now) {
  if (beepCount > 0) {
    if (!beepState) {
      if (now - beepTime > beepPause) {
        beepState = true;
        beepTime = now;
      }
    } else {
      if (now - beepTime > beepDuration) {
        beepState = false;
        beepTime = now;
        beepCount--;
      }
    } 
  }
  if (beepState) {
    tone(BEEP_PIN, BEEP_FREQ);
  } else {
    noTone(BEEP_PIN);
  }
}

void redrawScreen() {
  char text[50] = "",
       yStr[] = "y",
       nStr[] = "n",
       axisNames[][3] = {"AX", "AY", "BX", "BY"};
  Axis axis;

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
        status.battaryMV / 1000,
        status.battaryMV % 1000
      );
      break;
    case SCREEN_PROFILE:
      sprintf_P(
        text,
        PSTR("Profile\n%d"),
        currentProfile
      );
      break;
    case SCREEN_PIPE_ADDRESS:
      sprintf_P(
        text,
        PSTR("Pipe addr\n%s"),
        formatPipeAddress(settings.pipeAddressN)
      );
      break;
    case SCREEN_AUTO_CENTER:
      sprintf_P(
        text,
        PSTR("J centers\n%d,%d,\n%d,%d"),
        settings.axes[AXIS_A_X].joyCenter,
        settings.axes[AXIS_A_Y].joyCenter,
        settings.axes[AXIS_B_X].joyCenter,
        settings.axes[AXIS_B_Y].joyCenter
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
        settings.axes[axis].dualRate
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
        settings.axes[axis].trimming
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
        settings.axes[axis].joyInvert ? yStr : nStr
      );
      break;
    case SCREEN_CHANNEL_A_X:
    case SCREEN_CHANNEL_A_Y:
    case SCREEN_CHANNEL_B_X:
    case SCREEN_CHANNEL_B_Y:
      axis = screenNum - SCREEN_CHANNEL_A_X;
      if (settings.axes[axis].channel != NO_CHANNEL) {
        sprintf_P(
          text,
          PSTR("Channel %s\n%d"),
          axisNames[axis],
          settings.axes[axis].channel + 1
        );
      } else {
        sprintf_P(
          text,
          PSTR("Channel %s\nNone"),
          axisNames[axis]
        );
      }
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

void controlScreen(unsigned long now) {
  int settingsValueChange = 0;
  Screen prevScreenNum = screenNum;
  Axis axis;

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
        sendCommand(
          now,
          (settingsValueChange > 0) ?
          COMMAND_USER_COMMAND1 : COMMAND_USER_COMMAND2
        );
        break;
      case SCREEN_PROFILE:
        addWithConstrain(
          currentProfile, settingsValueChange, 0, NUM_PROFILES - 1
        );
        loadProfile();
        break;
      case SCREEN_PIPE_ADDRESS:
        addWithConstrain(
          settings.pipeAddressN, settingsValueChange, 0, 9
        );
        sendPipeAddress(now, settings.pipeAddressN);
        setPipeAddress(settings.pipeAddressN);
        break;
      case SCREEN_AUTO_CENTER:
        if (settingsValueChange > 0) {
          tone(BEEP_PIN, BEEP_FREQ);
          setJoystickCenter();
          noTone(BEEP_PIN);
        }
        break;
      case SCREEN_DUAL_RATE_A_X:
      case SCREEN_DUAL_RATE_A_Y:
      case SCREEN_DUAL_RATE_B_X:
      case SCREEN_DUAL_RATE_B_Y:
        axis = screenNum - SCREEN_DUAL_RATE_A_X;
        addWithConstrain(
          settings.axes[axis].dualRate,
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
          settings.axes[axis].trimming,
          settingsValueChange * 10,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_INVERT_A_X:
      case SCREEN_INVERT_A_Y:
      case SCREEN_INVERT_B_X:
      case SCREEN_INVERT_B_Y:
        axis = screenNum - SCREEN_INVERT_A_X;
        settings.axes[axis].joyInvert = settingsValueChange > 0;
        break;
      case SCREEN_CHANNEL_A_X:
      case SCREEN_CHANNEL_A_Y:
      case SCREEN_CHANNEL_B_X:
      case SCREEN_CHANNEL_B_Y:
        axis = screenNum - SCREEN_CHANNEL_A_X;
        addWithConstrain(
          settings.axes[axis].channel,
          settingsValueChange,
          NO_CHANNEL,
          NUM_CHANNELS - 1
        );
        break;
      case SCREEN_SAVE_FOR_NOLINK:
        if (settingsValueChange > 0) {
          sendCommand(now, COMMAND_SAVE_FOR_NOLINK);
          beepCount = 1;
          beepDuration = 250;
        }
        break;
      case SCREEN_SAVE:
        screenNum = NO_SCREEN;
        if (settingsValueChange > 0) {
          saveProfile();
          beepCount = 1;
          beepDuration = 500;
        }
        break;
    }
    redrawScreen();
  }
}

// vim:ai:sw=2:et
