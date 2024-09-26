#include <Arduino.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_Rx_Controller.h>

#ifndef TELEMETRY_INTERVAL
#define TELEMETRY_INTERVAL 5000
#endif

#ifndef FAILSAFE_TIMEOUT
#define FAILSAFE_TIMEOUT 1250
#endif

RxController::RxController(
    BaseRxSettings *settings,
    BaseReceiver *receiver,
    BaseOutput **outputs,
    VoltMetter *voltMetter,
    int pairPin,
    int ledPin
) : settings(settings)
  , receiver(receiver)
  , voltMetter(voltMetter)
  , pairPin(pairPin)
  , ledPin(ledPin)
{
  int i = 0;
  if (outputs != NULL)
    for(; i < NUM_CHANNELS && outputs[i] != NULL; i++)
      this->outputs[i] = outputs[i];
  for(; i < NUM_CHANNELS; i++)
    this->outputs[i] = NULL;

  isLedInverted = false;
}

bool RxController::begin() {
  BaseOutput *output;

  if (pairPin >= 0)
    pinMode(pairPin, INPUT_PULLUP);

  if (ledPin >= 0) {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, (isLedInverted) ? HIGH : LOW);
  }

  for (int i = 0; i < NUM_CHANNELS; i++)
    if ((output = outputs[i]) != NULL)
      output->begin();

  if (!settings->begin())
    return false;

  if (!settings->load()) {
    PRINTLN(F("No stored settings found, use defaults"));
    settings->setDefaults();
    settings->save();
  } else {
    PRINTLN(F("Using stored settings"));
  }

  if (
      !receiver->begin(
        &settings->values.address, settings->values.rfChannel, settings->values.paLevel
      )
  )
    return false;

  hasLastChannels = false;
  controlTime = 0;
  telemetryTime = 0;
  isFailsafe = false;

  return true;
}

void RxController::handle() {
  unsigned long now;
  union RequestPacket rp;

  if (receiver->receive(&rp)) {
    if (ledPin >= 0) 
      digitalWrite(ledPin, (isLedInverted) ? LOW : HIGH);

    handlePacket(&rp);

    if (ledPin >= 0) 
      digitalWrite(ledPin, (isLedInverted) ? HIGH : LOW);
  }

  now = millis();

  if (now - telemetryTime > TELEMETRY_INTERVAL) {
    sendTelemetry();
    telemetryTime = now;
  }

  if (!isFailsafe && controlTime > 0 && now - controlTime > FAILSAFE_TIMEOUT) {
    PRINTLN(F("Radio signal lost"));
    isFailsafe = true;

    applyControl(&settings->values.failsafe);
  }

  if (
      (pairPin >= 0 && digitalRead(pairPin) == LOW)
      || (pairPin < 0 && !receiver->isPaired())
  ) {
    if (receiver->pair()) {
      memcpy(
          settings->values.address.address,
          receiver->getPeerAddress()->address,
          ADDRESS_LENGTH
      );
      settings->save();
    }
  }
}

void RxController::handlePacket(const RequestPacket *rp) {
  ControlPacket *failsafe;

  if (rp->generic.packetType == PACKET_TYPE_CONTROL) {
    controlTime = millis();
    isFailsafe = false;

    for (int i = 0; i < NUM_CHANNELS; i++)
      lastChannels[i] = rp->control.channels[i];
    hasLastChannels = true;

#ifdef WITH_CONSOLE
    for (int i = 0; i < NUM_CHANNELS; i++) {
      PRINT(F("ch"));
      PRINT(i + 1);
      PRINT(F(": "));
      PRINT(rp->control.channels[i]);
      if (i < NUM_CHANNELS - 1) {
        PRINT(F(", "));
      } else {
        PRINTLN();
      }
    }
#endif

    applyControl(&rp->control);
  } else if (rp->generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
    PRINT(F("New RF channel: "));
    PRINTLN(rp->rfChannel.rfChannel);
    receiver->setRFChannel(rp->rfChannel.rfChannel);
    settings->values.rfChannel = rp->rfChannel.rfChannel;
    settings->save();
  } else if (rp->generic.packetType == PACKET_TYPE_SET_PA_LEVEL) {
    PRINT(F("New PA level: "));
    PRINTLN(rp->paLevel.paLevel);
    receiver->setPALevel(rp->paLevel.paLevel);
    settings->values.paLevel = rp->paLevel.paLevel;
    settings->save();
  } else if (rp->generic.packetType == PACKET_TYPE_COMMAND) {
    if (rp->command.command == COMMAND_SAVE_FAILSAFE && hasLastChannels) {
      PRINT(F("Saving state for failsafe"));
      failsafe = &settings->values.failsafe;
      failsafe->packetType = PACKET_TYPE_CONTROL;
      for (int i = 0; i < NUM_CHANNELS; i++)
        failsafe->channels[i] = lastChannels[i];
      settings->save();
    }
  }
}

void RxController::applyControl(const ControlPacket *control) {
  for (int i = 0; i < NUM_CHANNELS; i++)
    if (outputs[i] != NULL)
      outputs[i]->write(control->channels[i]);
}

void RxController::sendTelemetry() {
  unsigned int batteryMV;
  ResponsePacket resp;

  resp.telemetry.packetType = PACKET_TYPE_TELEMETRY;

  if (voltMetter != NULL) {
    batteryMV = voltMetter->readMillivolts();
  } else {
    batteryMV = 0;
  }
  PRINT(F("batteryMV: "));
  PRINTLN(batteryMV);
  resp.telemetry.batteryMV = batteryMV;

  receiver->send(&resp);
}

void RxController::setLedInverted(bool value) {
  isLedInverted = value;
}

// vim:ai:sw=2:et
