#include <LowcostRC_Console.h>
#include "Radio_Control.h"

RadioControl::RadioControl(Buzzer *buzzer) : radio(NULL), buzzer(buzzer) {
}

void RadioControl::begin() {
  radio = &nrf24Radio;

  if (nrf24Radio.begin()) {
    radio = &nrf24Radio;
  } else {
    spiRadio.begin();
    radio = &spiRadio;
  }
}

void RadioControl::sendRFChannel(int rfChannel) {
  union RequestPacket rp;
  rp.rfChannel.packetType = PACKET_TYPE_SET_RF_CHANNEL;
  rp.rfChannel.rfChannel = rfChannel;
  sendPacket(&rp);
}

void RadioControl::sendCommand(Command command) {
  union RequestPacket rp;
  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = command;
  sendPacket(&rp);
}

void RadioControl::sendPacket(union RequestPacket *packet) {
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
    
    buzzer->beep(BEEP_HIGH_HZ, 3, 3, 1);
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
}

// vim:et:sw=2:ai
