#include <string.h>
#include <Arduino.h>
#include <LowcostRC_Console.h>
#include "Config.h"
#include "Radio_NRF24.h"

// Phisical channels are 1..125
// 0 is alias to the default channel, eg 76
#define NRF24_NUM_RF_CHANNELS (125 + 1)
#define NRF24_DEFAULT_CHANNEL 76

#define NRF24_NUM_PA_LEVELS (RF24_PA_MAX-RF24_PA_MIN+1)

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
  rf24.setRadiation(RF24_PA_MIN, RF24_250KBPS);
  rf24.setPayloadSize(PACKET_SIZE);
  rf24.enableAckPayload();
  rf24.setRetries(5, 3);
  return true;
}

TxModuleType NRF24RadioModule::getModuleType() {
  return MODULE_TYPE_NRF24L01;
}

int NRF24RadioModule::getNumRFChannels() {
  return NRF24_NUM_RF_CHANNELS;
}

int NRF24RadioModule::getNumPALevels() {
  return NRF24_NUM_PA_LEVELS;
}

uint8_t NRF24RadioModule::rfChannelToNRF24(RFChannel ch) {
  if (ch == DEFAULT_RF_CHANNEL)
    return NRF24_DEFAULT_CHANNEL;
  return ch;
}

bool NRF24RadioModule::setPeer(const Address *addr) {
  memcpy(&peer, addr, sizeof(peer));
  rf24.stopListening();
  rf24.openWritingPipe(peer.address);
  rf24.enableAckPayload();
  return true;
}

bool NRF24RadioModule::setRFChannel(RFChannel ch) {
  rfChannel = ch;
  rf24.setChannel(rfChannelToNRF24(rfChannel));
  PRINT(F("NRF24: RF channel: "));
  PRINTLN(rfChannel);
  PRINT(F("NRF24: NRF24 channel: "));
  PRINTLN(rfChannelToNRF24(rfChannel));
  return true;
}

bool NRF24RadioModule::setPALevel(PALevel level) {
  level = constrain(level, 0, NRF24_NUM_PA_LEVELS - 1);
  rf24.setPALevel(level);
  PRINT(F("NRF24: PA level: "));
  PRINTLN(level);
  return true;
}

bool NRF24RadioModule::receive(union ResponsePacket *packet) {
  if (rf24.isAckPayloadAvailable()) {
    rf24.read(packet, sizeof(ResponsePacket));
    return true;
  }
  return false;
}

bool NRF24RadioModule::send(const union RequestPacket *packet) {
    return rf24.write(packet, sizeof(RequestPacket));
}

bool NRF24RadioModule::pair() {
  Address broadcast = ADDRESS_BROADCAST;
  RequestPacket req, resp;

  rf24.stopListening();
  rf24.openWritingPipe(broadcast.address);
  rf24.enableAckPayload();
  rf24.setChannel(NRF24_DEFAULT_CHANNEL);

  req.pair.packetType = PACKET_TYPE_PAIR;
  req.pair.session = random(1 << 15);
  req.pair.status = PAIR_STATUS_INIT;

  PRINT(F("NRF24: Starting pairing session: "));
  PRINTLN(req.pair.session);

  for (
    unsigned long start = millis();
    millis() - start < 3000;
    delay(5)
  ) {
    rf24.write(&req, sizeof(req));
    if (rf24.isAckPayloadAvailable()) {
      rf24.read(&resp, sizeof(resp));
      if (
          resp.pair.packetType == PACKET_TYPE_PAIR
          && resp.pair.session == req.pair.session
          && resp.pair.status == PAIR_STATUS_READY
      ) {
          PRINTLN(F("NRF24: Paired"));
          memcpy(&peer, &resp.pair.sender, sizeof(peer));
          req.pair.status = PAIR_STATUS_PAIRED;
          rf24.write(&req, sizeof(req));
          rf24.openWritingPipe(peer.address);
          rf24.setChannel(NRF24_DEFAULT_CHANNEL);
          return true;
      }
    }
  }

  PRINTLN(F("NRF24: Not paired"));
  rf24.openWritingPipe(peer.address);
  rf24.setChannel(rfChannelToNRF24(rfChannel));

  return false;
}

// vim:ai:sw=2:et
