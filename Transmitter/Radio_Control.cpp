#include <LowcostRC_Console.h>
#include "Radio_Control.h"

RadioControl::RadioControl(Buzzer *buzzer) : radio(NULL), buzzer(buzzer) {
  telemetry.battaryMV = 0;
}

void RadioControl::begin() {
  radio = NULL;
#ifdef WITH_RADIO_NRF24
  if (!radio && nrf24Radio.begin()) radio = &nrf24Radio;
#endif
#ifdef WITH_RADIO_SPI
  if (!radio && spiRadio.begin()) radio = &spiRadio;
#endif
}

void RadioControl::sendRFChannel(RFChannel channel) {
  union RequestPacket rp;
  rp.rfChannel.packetType = PACKET_TYPE_SET_RF_CHANNEL;
  rp.rfChannel.rfChannel = channel;
  sendPacket(&rp);
}

void RadioControl::sendCommand(Command command) {
  union RequestPacket rp;
  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = command;
  sendPacket(&rp);
}

void RadioControl::sendPacket(const union RequestPacket *packet) {
  static bool prevStatusRadioSuccess = false,
              prevStatusRadioFailure = false;
  bool radioOK, isStatusChanged;
  unsigned long now = millis();

  PRINT(F("Sending packet type: "));
  PRINT(packet->generic.packetType);
  PRINT(F("; size: "));
  PRINTLN(sizeof(*packet));

  radioOK = radio->send(packet);
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
    
    buzzer->beep(BEEP_HIGH_HZ, 5, 5, 1);
  }

  isStatusChanged = (
    statusRadioSuccess != prevStatusRadioSuccess
    || statusRadioFailure != prevStatusRadioFailure
  );
  prevStatusRadioSuccess = statusRadioSuccess;
  prevStatusRadioFailure = statusRadioFailure;

  if (isStatusChanged && statusRadioSuccess) {
    buzzer->beep(BEEP_LOW_HZ, 30, 30, 1);
  }
}

void RadioControl::handle() {
  ResponsePacket response;
  unsigned long now = millis();

  if (errorTime > 0 && now - errorTime > 250) {
    statusRadioFailure = false;
    errorTime = 0;
  }
  if (requestSendTime > 0 && now - requestSendTime > 250) {
    statusRadioSuccess = false;
  }

  if (radio->receive(&response)) {
    if (response.telemetry.packetType == PACKET_TYPE_TELEMETRY) {
      memcpy(&telemetry, &response.telemetry, sizeof(TelemetryPacket));
      telemetryTime = now;
      PRINT(F("Peer device battary (mV): "));
      PRINTLN(telemetry.battaryMV);
    }
  }
}

// vim:et:sw=2:ai
