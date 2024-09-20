#include <RF24.h>
#include <AbleButtons.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#define BUZZER_PIN A2

#define NRF24_CE_PIN 9
#define NRF24_CSN_PIN 10
#define NRF24_DEFAULT_CHANNEL 76

#ifndef PEER_ADDR
#define PEER_ADDR 0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef BATTERY_LOW_MV
#define BATTERY_LOW_MV 3400
#endif

#define CHANNEL1_UP_PIN 2
#define CHANNEL1_DOWN_PIN 4
#define CHANNEL1_CENTER_PIN 3
#define CHANNEL1_MIN_PIN 5

#define CHANNEL1_INIT 1000
#define CHANNEL1_MIN  1000
#define CHANNEL1_MAX  2000
#define CHANNEL1_STEP 50

enum Axis {
  AXIS_A_X,
  AXIS_A_Y,
  AXES_COUNT,
};

int joystickPins[] = {A0, A1};

struct AxisSettings {
  uint16_t joyCenter,
           joyThreshold;
  bool joyInvert;
  uint16_t dualRate;
  int16_t trimming;
  ChannelN channel;
};

struct SettingsValues {
  AxisSettings axes[AXES_COUNT];
};

const int DEFAULT_JOY_CENTER = 512,
          DEFAULT_JOY_THRESHOLD = 1;
const bool DEFAULT_JOY_INVERT = false;
const int CENTER_PULSE = 1500,
          DEFAULT_DUAL_RATE = 500,
          DEFAULT_TRIMMING = 0,
          DUAL_RATE_MIN = 10,
          DUAL_RATE_MAX = 1500,
          TRIMMING_MIN = -1500,
          TRIMMING_MAX = 1500;

SettingsValues settings = {
  {
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

unsigned long requestSendTime = 0,
              beepTime = 0;
bool statusRadioSuccess = false,
     statusRadioFailure = false,
     beepState = false;
int channel1Pulse = CHANNEL1_INIT;
int beepDuration = 0,
    beepPause = 0,
    beepCount = 0;

struct TelemetryPacket telemetry;

RF24 radio(NRF24_CE_PIN, NRF24_CSN_PIN);
const uint8_t peer[ADDRESS_LENGTH] = {PEER_ADDR};

using Button = AblePullupClickerButton;

Button channel1UpButton(CHANNEL1_UP_PIN),
       channel1DownButton(CHANNEL1_DOWN_PIN),
       channel1CenterButton(CHANNEL1_CENTER_PIN),
       channel1MinButton(CHANNEL1_MIN_PIN);

void controlLoop(unsigned long now);
int readAxis(Axis axis);
void setJoystickCenter();
void controlBuzzer(unsigned long now);

void setup(void)
{
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));

  for (int axis = 0; axis < AXES_COUNT; axis++) {
    pinMode(joystickPins[axis], INPUT);
  }

  channel1UpButton.begin();
  channel1UpButton.setDebounceTime(20);
  channel1DownButton.begin();
  channel1DownButton.setDebounceTime(20);
  channel1CenterButton.begin();
  channel1CenterButton.setDebounceTime(20);
  channel1MinButton.begin();
  channel1MinButton.setDebounceTime(20);

  pinMode(BUZZER_PIN, OUTPUT);

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN("Radio init: FAILURE");
  }
  radio.setRadiation(RF24_PA_LOW, RF24_250KBPS);
  radio.setPayloadSize(PACKET_SIZE);
  radio.enableAckPayload();

  radio.setChannel(NRF24_DEFAULT_CHANNEL);
  radio.openWritingPipe(peer);

  setJoystickCenter();
}

void loop(void) {
  unsigned long now = millis();

  controlLoop(now);
  controlBuzzer(now);
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

  channel1UpButton.handle();
  channel1DownButton.handle();
  channel1CenterButton.handle();
  channel1MinButton.handle();

  if (channel1UpButton.resetClicked()) {
    channel1Pulse = constrain(
      channel1Pulse + CHANNEL1_STEP, CHANNEL1_MIN, CHANNEL1_MAX
    );
  }
  if (channel1DownButton.resetClicked()) {
    channel1Pulse = constrain(
      channel1Pulse - CHANNEL1_STEP, CHANNEL1_MIN, CHANNEL1_MAX
    );
  }
  if (channel1CenterButton.resetClicked()) {
    channel1Pulse = (CHANNEL1_MIN + CHANNEL1_MAX) / 2;
  }
  if (channel1MinButton.resetClicked()) {
    channel1Pulse = CHANNEL1_MIN;
  }
  rp.control.channels[CHANNEL1] = channel1Pulse;

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    isChanged = isChanged || rp.control.channels[channel] != prevChannels[channel];

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    prevChannels[channel] = rp.control.channels[channel];

  if (
    isChanged
    || (requestSendTime > 0 && now - requestSendTime > 1000)
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
    requestSendTime = now;
  }

  if (radio.isAckPayloadAvailable()) {
    radio.read(&telemetry, sizeof(telemetry));
    if (telemetry.packetType == PACKET_TYPE_TELEMETRY) {
      PRINT(F("batteryMV: "));
      PRINTLN(telemetry.battaryMV);
      if (telemetry.battaryMV < BATTERY_LOW_MV) {
        beepCount = 3;
        beepDuration = 200;
        beepPause = 100;
      }
    }
  }
}

void sendRequest(unsigned long now, union RequestPacket *packet) {
  radio.write(packet, sizeof(*packet));
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

void controlBuzzer(unsigned long now) {
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
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// vim:ai:sw=2:et
