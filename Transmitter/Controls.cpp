#include <Arduino.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>
#include "Config.h"
#include "Controls.h"

const int CENTER_PULSE = 1500;

const int joystickPins[] = JOYSTICK_PINS;
const int switchPins[] = SWITCH_PINS;

Controls::Controls(Settings *settings, Buzzer *buzzer, RadioControl *radioControl)
  : settings(settings)
  , buzzer(buzzer)
  , radioControl(radioControl)
{
}

void Controls::begin() {
  for (int axis = 0; axis < AXES_COUNT; axis++) {
    pinMode(joystickPins[axis], INPUT);
  }
  for (int sw = 0; sw < SWITCHES_COUNT; sw++) {
    if (!IS_ANALOG_SWITCH(sw))
      pinMode(switchPins[sw], INPUT_PULLUP);
  }
}

int Controls::mapAxis(
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

int Controls::readAxis(Axis axis) {
  int joyValue = analogRead(joystickPins[axis]);
  return mapAxis(
    joyValue,
    settings->values.axes[axis].joyCenter,
    settings->values.axes[axis].joyThreshold,
    settings->values.axes[axis].joyInvert,
    settings->values.axes[axis].dualRate,
    settings->values.axes[axis].trimming
  );
}

void Controls::setJoystickCenter() {
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
    settings->values.axes[axis].joyCenter = value[axis] / count;
  }

  PRINTLN(F("DONE"));
}

int Controls::readSwitch(Switch sw) {
  if (IS_ANALOG_SWITCH(sw)) {
    return map(
      analogRead(switchPins[sw]),
      0, 1023,
      settings->values.switches[sw].low, settings->values.switches[sw].high
    );
  }
  else {
    return (
      (digitalRead(switchPins[sw]) == LOW) ?
      settings->values.switches[sw].high : settings->values.switches[sw].low
    );
  }
}

void Controls::handle() {
  union RequestPacket rp;
  bool isChanged = false,
       isPing,
       isRetry;
  static int prevChannels[NUM_CHANNELS];
  unsigned long now = millis();

  if (!radioControl->radio->isPaired()) return;

  rp.control.packetType = PACKET_TYPE_CONTROL;

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    rp.control.channels[channel] = 0;
  for (int axis = 0; axis < AXES_COUNT; axis++) {
    ChannelN channel = settings->values.axes[axis].channel;
    if (channel != NO_CHANNEL) {
      rp.control.channels[channel] = readAxis(axis);
    }
  }
  for (int sw = 0; sw < SWITCHES_COUNT; sw++) {
    ChannelN channel = settings->values.switches[sw].channel;
    if (channel != NO_CHANNEL) {
      rp.control.channels[channel] = readSwitch(sw);
    }
  }

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    isChanged = isChanged || rp.control.channels[channel] != prevChannels[channel];

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
    prevChannels[channel] = rp.control.channels[channel];

  isPing = (
    radioControl->errorTime == 0
    && radioControl->requestSendTime > 0
    && now - radioControl->requestSendTime > 1000
  );

  isRetry = (
    radioControl->errorTime > 0
    && now - radioControl->errorTime < 150
  );

  if (isChanged || isPing || isRetry) {
#ifdef WITH_CONSOLE
    PRINT(F("isChanged: "));
    PRINT(isChanged);
    PRINT(F("; isPing: "));
    PRINT(isPing);
    PRINT(F("; isRetry: "));
    PRINT(isRetry);

    for (int channel = 0; channel < NUM_CHANNELS; channel++) {
      PRINT(F("; ch"));
      PRINT(channel + 1);
      PRINT(F(": "));
      PRINT(rp.control.channels[channel]);
    }

    PRINTLN();
#endif

    radioControl->sendPacket(&rp);
  }
}

// vim:ai:sw=2:et
