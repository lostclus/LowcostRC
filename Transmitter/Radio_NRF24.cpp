#include <string.h>
#include <LowcostRC_Console.h>
#include "Config.h"
#include "Radio_NRF24.h"

NRF24RadioModule::NRF24RadioModule()
  : rf24(RADIO_NRF24_CE_PIN, RADIO_NRF24_CSN_PIN)
{
}

bool NRF24RadioModule::begin() {
  if (!rf24.begin()) {
    PRINTLN(F("NRF24: init: FAIL"));
    return false;
  }

  PRINTLN(F("NRF24: init: OK"));
  rf24.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  rf24.setPayloadSize(PACKET_SIZE);
  rf24.enableAckPayload();
  return true;
}

bool NRF24RadioModule::setRFChannel(int rfChannel) {
  byte pipe[7];

  rf24.setChannel(rfChannel);
  sprintf_P(pipe, PSTR(PIPE_FORMAT), rfChannel);
  rf24.openWritingPipe(pipe);

  PRINT(F("NRF24: RF channel: "));
  PRINTLN(rfChannel);
  return true;
}

bool NRF24RadioModule::receive(struct TelemetryPacket *telemetry) {
  if (rf24.isAckPayloadAvailable()) {
    rf24.read(telemetry, sizeof(TelemetryPacket));
    if (telemetry->packetType == PACKET_TYPE_TELEMETRY) {
	return true;
    }
  }
  return false;
}

bool NRF24RadioModule::send(union RequestPacket *packet) {
    return rf24.write(packet, sizeof(RequestPacket));
}

// vim:ai:sw=2:et
