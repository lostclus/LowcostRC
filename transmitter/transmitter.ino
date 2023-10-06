#include <string.h>
#include <EEPROM.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "protocol.h"

#undef WITH_CONSOLE
#define WITH_CONSOLE

#ifdef WITH_DISPLAY
#include "bitmaps.h"
#endif

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define ENGINE_OFF 0
#define ENGINE_MIN 20
#define ENGINE_MAX 255

#define BEEP_PIN 3
#define BEEP_FREQ 1320
#define SETTINGS_PIN 6
#define SETTINGS_PLUS_PIN 7
#define SETTINGS_MINUS_PIN 8

#define VOLT_METER_PIN A6
#define VOLT_METER_R1 10000L
#define VOLT_METER_R2 10000L

#define SETTINGS_MAGICK 0x5555
#define PROFILES_ADDR 0
#define NUM_PROFILES 5

enum JoystickAxis {
  JOYSTICK_A_X,
  JOYSTICK_A_Y,
  JOYSTICK_B_X,
  JOYSTICK_B_Y,
  JOYSTICK_AXES_COUNT,
};

int joystickPins[] = {A1, A0, A3, A2};

struct JoystickAxisSettings {
  int center,
      threshold,
      dualRate,
      trimming;
  bool invert;
};

struct Settings {
  int magick;
  JoystickAxisSettings joystick[JOYSTICK_AXES_COUNT];
};

const int JOYSTICK_LOGICAL_MAX = 16000,
          DEFAULT_CENTER = 512,
          DEFAULT_THRESHOLD = 1,
          DEFAULT_DUAL_RATE = 16000,
          DEFAULT_TRIMMING = 0,
          DUAL_RATE_MIN = 500,
          DUAL_RATE_MAX = 16000,
          TRIMMING_MIN = -8000,
          TRIMMING_MAX = 8000;
const bool DEFAULT_INVERT = false;

const Settings defaultSettings PROGMEM = {
  SETTINGS_MAGICK,
  {
    {
      DEFAULT_CENTER,
      DEFAULT_THRESHOLD,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      DEFAULT_INVERT
    },
    {
      DEFAULT_CENTER,
      DEFAULT_THRESHOLD,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      DEFAULT_INVERT
    },
    {
      DEFAULT_CENTER,
      DEFAULT_THRESHOLD,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      DEFAULT_INVERT
    },
    {
      DEFAULT_CENTER,
      DEFAULT_THRESHOLD,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      DEFAULT_INVERT
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
  SCREEN_SAVE,
  FIRST_SCREEN = NO_SCREEN,
  LAST_SCREEN = SCREEN_SAVE,
};

unsigned int thisBattaryMV = 0;

unsigned long requestSendTime = 0,
              errorTime = 0,
              settingsPressTime = 0,
              settingsPlusPressTime = 0,
              settingsMinusPressTime = 0,
              beepTime = 0,
              battaryUpdateTime = 0;
bool statusRadioSuccess = false,
     statusRadioFailure = false,
     settingsPress = false,
     settingsPlusPress = false,
     settingsMinusPress = false,
     beepState = false,
     engineOn = false;
int beepDuration = 0,
    beepPause = 0,
    beepCount = 0;
Screen screenNum = NO_SCREEN;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

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
void controlLoop(unsigned long now);
int readJoystickAxis(JoystickAxis axis);
void setJoystickCenter();
void updateBattaryVoltage();
void controlBeep(unsigned long now);
void redrawScreen();
void controlScreen(unsigned long now);

void setup(void)
{
  bool needsSetJoystickCenter = false;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));

  for (int axis = 0; axis < JOYSTICK_AXES_COUNT; axis++) {
    pinMode(joystickPins[axis], INPUT);
  }

  pinMode(SETTINGS_PIN, INPUT_PULLUP);
  pinMode(SETTINGS_PLUS_PIN, INPUT_PULLUP);
  pinMode(SETTINGS_MINUS_PIN, INPUT_PULLUP);
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
  radio.setPayloadSize(16);
  radio.enableAckPayload();
  radio.openWritingPipe(RADIO_PIPE);

  if (needsSetJoystickCenter) {
    setJoystickCenter();
  }

  request.magick = MAGICK;
  request.alerons = 0;
  request.elevator = 0;
  request.engine = 0;
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

void controlLoop(unsigned long now) {
  bool isChanged;
  int prevAlerons = request.alerons,
      prevElevator = request.elevator;
  byte prevEngine = request.engine;

  request.alerons = readJoystickAxis(JOYSTICK_B_X);
  request.elevator = readJoystickAxis(JOYSTICK_B_Y);

  int joyAY = readJoystickAxis(JOYSTICK_A_Y),
      engineToogleThreshold  = settings.joystick[JOYSTICK_A_Y].dualRate / 4;
  if (engineOn) {
    if (joyAY < -engineToogleThreshold) {
      engineOn = false;
      request.engine = ENGINE_OFF;
    } else {
      request.engine = map(max(0, joyAY), 0, JOYSTICK_LOGICAL_MAX, ENGINE_MIN, ENGINE_MAX);
    }
  } else if (joyAY > engineToogleThreshold) {
    engineOn = true;
    request.engine = ENGINE_MIN;
  }

  isChanged = (
    request.alerons != prevAlerons
    || request.elevator != prevElevator
    || request.engine != prevEngine
  );

  if (
    isChanged
    || (requestSendTime > 0 && now - requestSendTime > 1000)
    || (errorTime > 0 && now - errorTime < 200)
  ) {
    PRINT(F("alerons: "));
    PRINT(request.alerons);
    PRINT(F("; elevator: "));
    PRINT(request.elevator);
    PRINT(F("; engine: "));
    PRINT(request.engine);

    sendRequest(now);
  }

  if (radio.isAckPayloadAvailable()) {
    radio.read(&response, sizeof(response));
    if (response.magick == MAGICK) {
      PRINT(F("battaryMV: "));
      PRINTLN(response.battaryMV);
      if (response.battaryMV < 3400) {
        beepCount = 3;
        beepDuration = 200;
        beepPause = 100;
      }
    }
  }
}

void sendRequest(unsigned long now) {
  static bool prevStatusRadioSuccess = false,
              prevStatusRadioFailure = false;
  bool radioOK, isStatusChanged;

  radioOK = radio.write(&request, sizeof(request));
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

int mapJoystickAxis(int value, int center, int threshold, int dualRate, int trimming) {
  if (value >= center - threshold && value <= center + threshold) value = 0;
  else if (value < center)
    value = map(value, 0, center - threshold, -dualRate, 0);
  else if (value > center)
    value = map(value, center + threshold, 1023, 0, dualRate);
  value += trimming;
  return value;
}

int readJoystickAxis(JoystickAxis axis) {
  int rawValue = analogRead(joystickPins[axis]);
  int value = mapJoystickAxis(
    rawValue,
    settings.joystick[axis].center,
    settings.joystick[axis].threshold,
    settings.joystick[axis].dualRate,
    settings.joystick[axis].trimming
  );
  if (settings.joystick[axis].invert)
    value = -value;
  return value;
}

void setJoystickCenter() {
  int value[JOYSTICK_AXES_COUNT] = {0, 0, 0, 0},
      count = 5;

  PRINT(F("Setting joystick center..."));

  for (int i = 0; i < count; i++) {
    for (int axis = 0; axis < JOYSTICK_AXES_COUNT; axis++) {
      value[axis] += analogRead(joystickPins[axis]);
    }
    delay(100);
  }

  for (int axis = 0; axis < JOYSTICK_AXES_COUNT; axis++) {
    settings.joystick[axis].center = value[axis] / count;
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
       nStr[] = "n";

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
        response.battaryMV / 1000,
        response.battaryMV % 1000
      );
      break;
    case SCREEN_PROFILE:
      sprintf_P(
        text,
        PSTR("Profile\n%d"),
        currentProfile
      );
      break;
    case SCREEN_AUTO_CENTER:
      sprintf_P(
        text,
        PSTR("Centers\n%d,%d,\n%d,%d"),
        settings.joystick[JOYSTICK_A_X].center,
        settings.joystick[JOYSTICK_A_Y].center,
        settings.joystick[JOYSTICK_B_X].center,
        settings.joystick[JOYSTICK_B_Y].center
      );
      break;
    case SCREEN_DUAL_RATE_A_X:
      sprintf_P(
        text,
        PSTR("D/R AX\n%d"),
        settings.joystick[JOYSTICK_A_X].dualRate
      );
      break;
    case SCREEN_DUAL_RATE_A_Y:
      sprintf_P(
        text,
        PSTR("D/R AY\n%d"),
        settings.joystick[JOYSTICK_A_Y].dualRate
      );
      break;
    case SCREEN_DUAL_RATE_B_X:
      sprintf_P(
        text,
        PSTR("D/R BX\n%d"),
        settings.joystick[JOYSTICK_B_X].dualRate
      );
      break;
    case SCREEN_DUAL_RATE_B_Y:
      sprintf_P(
        text,
        PSTR("D/R BY\n%d"),
        settings.joystick[JOYSTICK_B_Y].dualRate
      );
      break;
    case SCREEN_TRIMMING_A_X:
      sprintf_P(
        text,
        PSTR("Tr AX\n%d"),
        settings.joystick[JOYSTICK_A_X].trimming
      );
      break;
    case SCREEN_TRIMMING_A_Y:
      sprintf_P(
        text,
        PSTR("Tr AY\n%d"),
        settings.joystick[JOYSTICK_A_Y].trimming
      );
      break;
    case SCREEN_TRIMMING_B_X:
      sprintf_P(
        text,
        PSTR("Tr BX\n%d"),
        settings.joystick[JOYSTICK_B_X].trimming
      );
      break;
    case SCREEN_TRIMMING_B_Y:
      sprintf_P(
        text,
        PSTR("Tr BY\n%d"),
        settings.joystick[JOYSTICK_B_Y].trimming
      );
      break;
    case SCREEN_INVERT_A_X:
      sprintf_P(
        text,
        PSTR("Invert AX\n%s"),
        settings.joystick[JOYSTICK_A_X].invert ? yStr : nStr
      );
      break;
    case SCREEN_INVERT_A_Y:
      sprintf_P(
        text,
        PSTR("Invert AY\n%s"),
        settings.joystick[JOYSTICK_A_Y].invert ? yStr : nStr
      );
      break;
    case SCREEN_INVERT_B_X:
      sprintf_P(
        text,
        PSTR("Invert BX\n%s"),
        settings.joystick[JOYSTICK_B_X].invert ? yStr : nStr
      );
      break;
    case SCREEN_INVERT_B_Y:
      sprintf_P(
        text,
        PSTR("Invert BY\n%s"),
        settings.joystick[JOYSTICK_B_Y].invert ? yStr : nStr
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
  bool prevSettingsPress = settingsPress,
       prevSettingsPlusPress = settingsPlusPress,
       prevSettingsMinusPress = settingsMinusPress;
  int settingsValueChange = 0;
  Screen prevScreenNum = screenNum;

  settingsPress = digitalRead(SETTINGS_PIN) == LOW;
  settingsPlusPress = digitalRead(SETTINGS_PLUS_PIN) == LOW;
  settingsMinusPress = digitalRead(SETTINGS_MINUS_PIN) == LOW;

  if (settingsPress && !prevSettingsPress) {
    settingsPressTime = now;
    screenNum = screenNum + ((Screen)1);
    if (screenNum > LAST_SCREEN)
      screenNum = FIRST_SCREEN;
    PRINT(F("Screen: "));
    PRINTLN(screenNum);
    redrawScreen();
  }

  if (!settingsPress)
    settingsPressTime = 0;

  if (settingsPressTime > 0 && now - settingsPressTime > 2000) {
    screenNum = NO_SCREEN;
    PRINT(F("Screen: "));
    PRINTLN(screenNum);
    redrawScreen();
    settingsPressTime = 0;
  }

  if (settingsPlusPress && !prevSettingsPlusPress) {
    settingsValueChange = 1;
  }
  if (settingsMinusPress && !prevSettingsMinusPress) {
    settingsValueChange = -1;
  }

  if (settingsValueChange != 0) {
    switch (screenNum) {
      case SCREEN_PROFILE:
        addWithConstrain(
          currentProfile, settingsValueChange, 0, NUM_PROFILES - 1
        );
        loadProfile();
        break;
      case SCREEN_AUTO_CENTER:
        if (settingsValueChange > 0) {
          tone(BEEP_PIN, BEEP_FREQ);
          setJoystickCenter();
          noTone(BEEP_PIN);
        }
        break;
      case SCREEN_DUAL_RATE_A_X:
        addWithConstrain(
          settings.joystick[JOYSTICK_A_X].dualRate,
          settingsValueChange * 250,
          DUAL_RATE_MIN,
          DUAL_RATE_MAX
        );
        break;
      case SCREEN_DUAL_RATE_A_Y:
        addWithConstrain(
          settings.joystick[JOYSTICK_A_Y].dualRate,
          settingsValueChange * 250,
          DUAL_RATE_MIN,
          DUAL_RATE_MAX
        );
        break;
      case SCREEN_DUAL_RATE_B_X:
        addWithConstrain(
          settings.joystick[JOYSTICK_B_X].dualRate,
          settingsValueChange * 250,
          DUAL_RATE_MIN,
          DUAL_RATE_MAX
        );
        break;
      case SCREEN_DUAL_RATE_B_Y:
        addWithConstrain(
          settings.joystick[JOYSTICK_B_Y].dualRate,
          settingsValueChange * 250,
          DUAL_RATE_MIN,
          DUAL_RATE_MAX
        );
        break;
      case SCREEN_TRIMMING_A_X:
        addWithConstrain(
          settings.joystick[JOYSTICK_A_X].trimming,
          settingsValueChange * 100,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_TRIMMING_A_Y:
        addWithConstrain(
          settings.joystick[JOYSTICK_A_Y].trimming,
          settingsValueChange * 100,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_TRIMMING_B_X:
        addWithConstrain(
          settings.joystick[JOYSTICK_B_X].trimming,
          settingsValueChange * 100,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_TRIMMING_B_Y:
        addWithConstrain(
          settings.joystick[JOYSTICK_B_Y].trimming,
          settingsValueChange * 100,
          TRIMMING_MIN,
          TRIMMING_MAX
        );
        break;
      case SCREEN_INVERT_A_X:
        settings.joystick[JOYSTICK_A_X].invert = settingsValueChange > 0;
        break;
      case SCREEN_INVERT_A_Y:
        settings.joystick[JOYSTICK_A_Y].invert = settingsValueChange > 0;
        break;
      case SCREEN_INVERT_B_X:
        settings.joystick[JOYSTICK_B_X].invert = settingsValueChange > 0;
        break;
      case SCREEN_INVERT_B_Y:
        settings.joystick[JOYSTICK_B_Y].invert = settingsValueChange > 0;
        break;
      case SCREEN_SAVE:
        screenNum = NO_SCREEN;
        if (settingsValueChange > 0) {
          saveProfile();
          beepCount = 1;
          beepDuration = 500;
        }
    }
    redrawScreen();
  }
}

// vim:ai:sw=2:et
