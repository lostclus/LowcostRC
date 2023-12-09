#include <string.h>
#include <EEPROM.h>
#include <RF24.h>
#include <AbleButtons.h>

#include <LowcostRC_Protocol.h>

#undef WITH_CONSOLE
#define WITH_CONSOLE

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define BEEP_PIN A2

#define SETTINGS_MAGICK 0x5555
#define SETTINGS_ADDR 0

enum Axis {
  AXIS_A_X,
  AXIS_A_Y,
  AXES_COUNT,
};

int joystickPins[] = {A0, A1};

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
  int rfChannel;
  AxisSettings axes[AXES_COUNT];
};

const int DEFAULT_JOY_CENTER = 512,
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
  78, // DEFAULT_RF_CHANNEL,
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
    }
  }
};

Settings settings;
#define SETTINGS_SIZE sizeof(settings)

#define CHANNEL1_UP_PIN 2
#define CHANNEL1_DOWN_PIN 3
#define CHANNEL1_CENTER_PIN 4

#define CHANNEL1_MIN 1000
#define CHANNEL1_MAX 2000
#define CHANNEL1_STEP 50

unsigned long requestSendTime = 0,
              errorTime = 0,
              engineAfterburnerPressTime = 0,
              beepTime = 0;
bool statusRadioSuccess = false,
     statusRadioFailure = false,
     beepState = false;
int channel3Pulse = (CHANNEL1_MIN + CHANNEL1_MAX) / 2;
int beepDuration = 0,
    beepPause = 0,
    beepCount = 0;

struct StatusPacket status;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

Button channel3UpButton(CHANNEL1_DOWN_PIN),
       channel3DownButton(CHANNEL1_UP_PIN),
       channel3CenterButton(CHANNEL1_CENTER_PIN);

Button *buttonsArray[] = {
  &channel3UpButton,
  &channel3DownButton,
  &channel3CenterButton
};
ButtonList buttons(buttonsArray);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

bool loadProfile();
void setRFChannel(int rfChannel);
void controlLoop(unsigned long now);
int readAxis(Axis axis);
void setJoystickCenter();
void controlBeep(unsigned long now);

void setup(void)
{
  bool needsSetJoystickCenter = false;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));

  for (int axis = 0; axis < AXES_COUNT; axis++) {
    pinMode(joystickPins[axis], INPUT);
  }

  buttons.begin();
  channel3UpButton.setDebounceTime(20);
  channel3DownButton.setDebounceTime(20);
  channel3CenterButton.setDebounceTime(20);

  pinMode(BEEP_PIN, OUTPUT);

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

  setRFChannel(settings.rfChannel);

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

  controlBeep(now);
}

bool loadProfile() {
  PRINT(F("Reading settings"));
  PRINTLN(F(" from flash ROM..."));
  EEPROM.get(SETTINGS_ADDR, settings);

  if (true /*settings.magick != SETTINGS_MAGICK*/) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&settings, &defaultSettings, SETTINGS_SIZE);
    return false;
  }

  PRINTLN(F("Using stored settings in flash ROM"));
  return true;
}

void setRFChannel(int rfChannel) {
  byte pipe[7];

  radio.setChannel(rfChannel);
  sprintf_P(pipe, PSTR(PIPE_FORMAT), rfChannel);
  radio.openWritingPipe(pipe);

  PRINT(F("RF channel: "));
  PRINTLN(rfChannel);
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

  if (channel3UpButton.resetClicked()) {
    channel3Pulse = constrain(
      channel3Pulse + CHANNEL1_STEP, CHANNEL1_MIN, CHANNEL1_MAX
    );
  }
  if (channel3DownButton.resetClicked()) {
    channel3Pulse = constrain(
      channel3Pulse - CHANNEL1_STEP, CHANNEL1_MIN, CHANNEL1_MAX
    );
  }
  if (channel3CenterButton.resetClicked()) {
    channel3Pulse = (CHANNEL1_MIN + CHANNEL1_MAX) / 2;
  }
  rp.control.channels[CHANNEL3] = channel3Pulse;

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    isChanged = isChanged || rp.control.channels[channel] != prevChannels[channel];

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    prevChannels[channel] = rp.control.channels[channel];

  if (
    isChanged
    /*
    || (requestSendTime > 0 && now - requestSendTime > 1000)
    || (errorTime > 0 && now - errorTime < 200)
    */
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
  int value[AXES_COUNT] = {0, 0},
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
    digitalWrite(BEEP_PIN, HIGH);
  } else {
    digitalWrite(BEEP_PIN, LOW);
  }
}

// vim:ai:sw=2:et
