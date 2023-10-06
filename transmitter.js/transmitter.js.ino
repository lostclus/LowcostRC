#include <EEPROM.h>
#include <RF24.h>

#include "protocol.h"

#undef WITH_CONSOLE
#define WITH_CONSOLE

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define JOYSTICK_X_PIN 0
#define JOYSTICK_Y_PIN 1
#define JOYSTICK_X_CENTER 512
#define JOYSTICK_Y_CENTER 512
#define JOYSTICK_X_THRESHOLD 0
#define JOYSTICK_Y_THRESHOLD 0

#undef JOYSTICK_INVERT_X
#define JOYSTICK_INVERT_Y
#undef JOYSTICK_SWAP_AXIS

#define ENGINE_UP_PIN 2
#define ENGINE_DOWN_PIN 3
#define ENGINE_OFF_PIN 4
#define ENGINE_AFTERBURNER_PIN 5

#define ENGINE_OFF 0
#define ENGINE_MIN 20
#define ENGINE_AFTERBURNER_THRESHOLD 240
#define ENGINE_MAX 255

#define BEEP_PIN A2
#define TRIM_SET_PIN 6
#define ALERONS_TRIM_PIN 3
#define ELEVATOR_TRIM_PIN 4

#define SETTINGS_MAGICK 0x5555
#define SETTINGS_ADDR 0

struct Settings {
  int magick;
  int joystick_x_center,
      joystick_y_center,
      joystick_x_threshold,
      joystick_y_threshold,
      aleronsShift,
      aleronsTrim,
      elevatorShift,
      elevatorTrim;
};

const Settings defaultSettings PROGMEM = {
  SETTINGS_MAGICK,
  JOYSTICK_X_CENTER,
  JOYSTICK_Y_CENTER,
  JOYSTICK_X_THRESHOLD,
  JOYSTICK_Y_THRESHOLD,
  0, 16,
  0, 16,
};

Settings settings;

unsigned long requestSendTime = 0,
              errorTime = 0,
              engineAfterburnerPressTime = 0,
              beepTime = 0;
bool statusRadioSuccess = false,
     statusRadioFailure = false,
     engineUpPress = false,
     engineDownPress = false,
     beepState = false;
int beepDuration = 0,
    beepPause = 0,
    beepCount = 0;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

void controlLoop(unsigned long now);
int mapJoystickDimension(int value, int center, int threshold);
void readJoystick(int &joyx, int &joyy);
void calibrateJoystick();
void controlBeep(unsigned long now);
void controlTrim(bool force);

void setup(void)
{
  bool needsCalibrateJoystick = false;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif

  pinMode(JOYSTICK_X_PIN, INPUT);
  pinMode(JOYSTICK_Y_PIN, INPUT);

  pinMode(ENGINE_UP_PIN, INPUT_PULLUP);
  pinMode(ENGINE_DOWN_PIN, INPUT_PULLUP);
  pinMode(ENGINE_OFF_PIN, INPUT_PULLUP);
  pinMode(ENGINE_AFTERBURNER_PIN, INPUT_PULLUP);
  pinMode(BEEP_PIN, OUTPUT);

  PRINTLN(F("Reading settings from flash ROM..."));
  EEPROM.get(SETTINGS_ADDR, settings);
  if (settings.magick != SETTINGS_MAGICK) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&settings, &defaultSettings, sizeof(settings));
    needsCalibrateJoystick = true;
  } else {
    PRINTLN(F("Using stored settings in flash ROM"));
  }
  // settings.aleronsTrim = 3;
  // settings.elevatorTrim = 8;
  controlTrim(true);

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN("Radio init: FAILURE");
  }
  radio.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  radio.setPayloadSize(16);
  radio.enableAckPayload();
  radio.openWritingPipe(RADIO_PIPE);

  if (digitalRead(ENGINE_DOWN_PIN) == LOW || needsCalibrateJoystick) {
    calibrateJoystick();
  }

  request.magick = MAGICK;
  request.alerons = 0;
  request.elevator = 0;
  request.engine = 0;
}

void loop(void) {
  unsigned long now = millis();

  controlTrim(false);
  controlLoop(now);

  if (errorTime > 0 && now - errorTime > 250) {
    statusRadioFailure = false;
    errorTime = 0;
  }
  if (requestSendTime > 0 && now - requestSendTime > 250) {
    statusRadioSuccess = false;
  }

  controlBeep(now);
}

void controlLoop(unsigned long now) {
  int joyx, joyy;
  bool isChanged;
  int prevAlerons = request.alerons,
      prevElevator = request.elevator,
      engineDelta = 0;
  byte prevEngine = request.engine;
  bool engineOffPress,
       prevEngineUpPress = engineUpPress,
       prevEngineDownPress = engineDownPress;

  readJoystick(joyx, joyy);
  request.alerons = (joyx + settings.aleronsShift) / 16 * settings.aleronsTrim;
  request.elevator = (joyy + settings.elevatorShift) / 16 * settings.elevatorTrim;

  engineOffPress = digitalRead(ENGINE_OFF_PIN) == LOW;
  engineUpPress = digitalRead(ENGINE_UP_PIN) == LOW;
  engineDownPress = digitalRead(ENGINE_DOWN_PIN) == LOW;
  if (digitalRead(ENGINE_AFTERBURNER_PIN) == LOW) {
    engineAfterburnerPressTime = now;
  }

  if (engineUpPress && !prevEngineUpPress) {
    engineDelta = 10;
  }
  if (engineDownPress && !prevEngineDownPress) {
    engineDelta = -10;
  }

  if (engineAfterburnerPressTime > 0) {
    if (now - engineAfterburnerPressTime < 10000) {
      if (engineDownPress) {
        request.engine = ENGINE_AFTERBURNER_THRESHOLD;
        engineAfterburnerPressTime = 0;
      } else if (engineOffPress) {
        request.engine = ENGINE_OFF;
        engineAfterburnerPressTime = 0;
      } else {
        request.engine = ENGINE_MAX;
      }
    } else {
      request.engine = ENGINE_AFTERBURNER_THRESHOLD;
      engineAfterburnerPressTime = 0;
    }
  }
  if (engineOffPress) {
    request.engine = ENGINE_OFF;
  } else if (engineDelta) {
    request.engine = max(
      ENGINE_MIN,
      min(
        ENGINE_AFTERBURNER_THRESHOLD,
        request.engine + engineDelta
      )
    ); 
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
    PRINT(F("; alerons trim: "));
    PRINT(settings.aleronsTrim);
    PRINT(F("; elevator trim: "));
    PRINTLN(settings.elevatorTrim);

    sendRequest(now);
  }

  if (radio.isAckPayloadAvailable()) {
    radio.read(&response, sizeof(response));
    if (response.magick == MAGICK) {
      PRINT(F("battaryMV: "));
      PRINTLN(response.battaryMV);
      if (response.battaryMV < 3100) {
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

  isStatusChanged = (statusRadioSuccess != prevStatusRadioSuccess ||
                     statusRadioFailure != prevStatusRadioFailure);
  prevStatusRadioSuccess = statusRadioSuccess;
  prevStatusRadioFailure = statusRadioFailure;

  if (isStatusChanged && statusRadioSuccess) {
    beepCount = 1;
    beepDuration = 50;
    beepPause = 50;
  }
}

int mapJoystickDimension(int value, int center, int threshold) {
  if (value >= center - threshold &&
      value <= center + threshold) value = 0;
  else if (value < center)
    value = map(value, 0, center - threshold, -16000, 0);
  else if (value > center)
    value = map(value, center + threshold, 1023, 0, 16000);
  return value;
}

void readJoystick(int &joyx, int &joyy) {
  joyx = analogRead(JOYSTICK_X_PIN);
  joyy = analogRead(JOYSTICK_Y_PIN);

  joyx = mapJoystickDimension(
    joyx,
    settings.joystick_x_center,
    settings.joystick_x_threshold);
  joyy = mapJoystickDimension(
    joyy,
    settings.joystick_y_center,
    settings.joystick_y_threshold);

  #ifdef JOYSTICK_INVERT_X
  joyx = -joyx;
  #endif
  #ifdef JOYSTICK_INVERT_Y
  joyy = -joyy;
  #endif
  #ifdef JOYSTICK_SWAP_AXIS
  int joyswp = joyx;
  joyx = joyy; joyy = joyswp;
  #endif
}

void calibrateJoystick() {
  int joyx = 0,
      joyy = 0,
      count = 5;

  PRINT(F("Calibrating joystick..."));
  for (int i = 0; i < count; i++) {
    joyx += analogRead(JOYSTICK_X_PIN);
    joyy += analogRead(JOYSTICK_Y_PIN);
    delay(200);
  }

  settings.joystick_x_center = joyx / count;
  settings.joystick_y_center = joyy / count;

  EEPROM.put(SETTINGS_ADDR, settings);
  PRINTLN(F("DONE"));
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
  digitalWrite(BEEP_PIN, beepState ? HIGH : LOW);
}

void controlTrim(bool force) {
  if (force || digitalRead(TRIM_SET_PIN) == LOW) {
    settings.aleronsTrim = map(analogRead(ALERONS_TRIM_PIN), 0, 1023, 1, 16);
    settings.elevatorTrim = map(analogRead(ELEVATOR_TRIM_PIN), 0, 1023, 1, 16);
  }
}

// vim:ai:sw=2:et
