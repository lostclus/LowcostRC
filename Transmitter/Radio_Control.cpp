#include <LowcostRC_Console.h>
#include "Radio_Control.h"

RadioControl::RadioControl(Buzzer *buzzer) : radio(NULL), buzzer(buzzer) {
  telemetry.battaryMV = 0;
}

void RadioControl::begin() {
  radio = NULL;
#ifdef WITH_RADIO_NRF24
  if (!radio) {
    radio = new NRF24RadioModule();
    if (!radio->begin()) {
      delete radio;
      radio = NULL;
    }
  }
#endif
#ifdef WITH_RADIO_SPI
  if (!radio) {
    radio = new SPIRadioModule();
    if (!radio->begin()) {
      delete radio;
      radio = NULL;
    }
  }
#endif
}

void RadioControl::sendRFChannel(RFChannel channel) {
  union RequestPacket rp;
  rp.rfChannel.packetType = PACKET_TYPE_SET_RF_CHANNEL;
  rp.rfChannel.rfChannel = channel;
  sendPacket(&rp);
}

void RadioControl::sendPALevel(PALevel level) {
  union RequestPacket rp;
  rp.paLevel.packetType = PACKET_TYPE_SET_PA_LEVEL;
  rp.paLevel.paLevel = level;
  sendPacket(&rp);
}

void RadioControl::sendCommand(Command command) {
  union RequestPacket rp;
  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = command;
  sendPacket(&rp);
}

void RadioControl::sendPacket(const union RequestPacket *packet) {
  unsigned long now = millis();

  PRINT(F("Sending packet type: "));
  PRINT(packet->generic.packetType);
  PRINT(F("; size: "));
  PRINTLN(sizeof(*packet));

  packetsCount++;

  if (radio->send(packet)) {
    requestSendTime = now;
    errorTime = 0;
  } else {
    if (errorTime == 0) errorTime = now;
    requestSendTime = 0;
    packetsFailureCount++;
  }

  if (packetsCount == 100) {
    prevLinkQuality = linkQuality;
    linkQuality = 100 - packetsFailureCount;
    packetsCount = 0;
    packetsFailureCount = 0;
    if (linkQuality < MIN_LINK_QUALITY) {
      buzzer->beep(BEEP_HIGH_HZ, 5, 5, 1);
    } else if (prevLinkQuality < MIN_LINK_QUALITY) {
      buzzer->beep(BEEP_LOW_HZ, 30, 30, 1);
    }
  }
}

void RadioControl::handle() {
  ResponsePacket response;
  unsigned long now = millis();

  if (errorTime > 0 && now - errorTime > 250) {
    errorTime = 0;
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
