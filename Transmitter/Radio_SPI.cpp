#include <string.h>
#include <SPI.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_SPI.h>

#include "Radio_SPI.h"

#define SS_PIN 10
#define INIT_TIMEOUT 2000
#define INIT_RETRY_COUNT 5
#define INIT_RETRY_PAUSE 500
#define SEND_TIMEOUT 500
#define PAIR_TIMEOUT 5000


SPIRadioModule::SPIRadioModule() {
}

void SPIRadioModule::pulseSS() {
    digitalWrite(SS_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(SS_PIN, LOW);
}

uint32_t SPIRadioModule::readStatus() {
  pulseSS();
  SPI.transfer(0x04);
  uint32_t status = (SPI.transfer(0) | ((uint32_t)(SPI.transfer(0)) << 8) | ((uint32_t)(SPI.transfer(0)) << 16) | ((uint32_t)(SPI.transfer(0)) << 24));
  pulseSS();
  return status;
}

void SPIRadioModule::writeStatus(uint32_t status) {
  pulseSS();
  SPI.transfer(0x01);
  SPI.transfer(status & 0xFF);
  SPI.transfer((status >> 8) & 0xFF);
  SPI.transfer((status >> 16) & 0xFF);
  SPI.transfer((status >> 24) & 0xFF);
  pulseSS();
}

void SPIRadioModule::readData(uint8_t *data) {
  pulseSS();
  SPI.transfer(0x03);
  SPI.transfer(0x00);
  for (uint8_t i = 0; i < SPI_PACKET_SIZE; i++) { data[i] = SPI.transfer(0); }
  pulseSS();
}

void SPIRadioModule::writeData(uint8_t *data, size_t len) {
  uint8_t i = 0;
  pulseSS();
  SPI.transfer(0x02);
  SPI.transfer(0x00);
  while (len-- && i < SPI_PACKET_SIZE) { SPI.transfer(data[i++]); }
  while (i++ < SPI_PACKET_SIZE) { SPI.transfer(0); }
  pulseSS();
}

bool SPIRadioModule::begin() {
  uint32_t newStatus;

  pinMode(SS_PIN, OUTPUT);
  SPI.begin();

  for (int i = 0; i < INIT_RETRY_COUNT; i++) {
    writeStatus(SPI_STATUS_STARTING);
    for (
        unsigned long start = millis();
        millis() - start < INIT_TIMEOUT;
        delay(1)
    ) {
      newStatus = readStatus();
      if (newStatus != SPI_STATUS_STARTING) break;
    }
    if (newStatus == SPI_STATUS_OK) break;
    delay(INIT_RETRY_PAUSE);
  }

  if (newStatus != SPI_STATUS_OK) {
    PRINTLN(F("SPI: Init: FAIL"));
    PRINTLN(newStatus);
    return false;
  }
  PRINTLN(F("SPI: Init: OK"));
  return true;
}

bool SPIRadioModule::setPeer(const Address *addr) {
  SPIRequestPacket req;

  req.peerAddr.packetType = SPI_PACKET_TYPE_SET_PEER_ADDRESS;
  memcpy(req.peerAddr.peer.address, addr->address, ADDRESS_LENGTH);

  if (sendGeneric(&req, sizeof(SPIRequestPacket), SPI_STATUS_SET_PEER_ADDRESS)) {
    memcpy(peer.address, addr->address, ADDRESS_LENGTH);
    return true;
  }
  return false;
}

bool SPIRadioModule::setRFChannel(RFChannel ch) {
  SPIRequestPacket req;

  req.rfChannel.packetType = SPI_PACKET_TYPE_SET_RF_CHANNEL;
  req.rfChannel.rfChannel = ch;

  if (sendGeneric(&req, sizeof(SPIRequestPacket), SPI_STATUS_SET_CHANNEL)) {
    rfChannel = ch;
    return true;
  }
  return false;
}

bool SPIRadioModule::receiveGeneric(void *data, size_t size, uint32_t status) {
  uint8_t buf[SPI_PACKET_SIZE];

  if (readStatus() == status) {
    memset(buf, 0, sizeof(buf));
    readData(buf);
    memcpy(data, buf, size);
    writeStatus(SPI_STATUS_OK);
    return true;
  }
  return false;
}

bool SPIRadioModule::receive(union ResponsePacket *packet) {
  return receiveGeneric(packet, sizeof(ResponsePacket), SPI_STATUS_RECEIVING);
}

bool SPIRadioModule::sendGeneric(const void *data, size_t size, uint32_t status) {
  uint32_t newStatus;

  writeStatus(status);
  writeData((uint8_t*)data, size);
  for (
    unsigned long start = millis();
    millis() - start < SEND_TIMEOUT;
    delay(1)
  ) {
    newStatus = readStatus();
    if (newStatus != status) break;
  }

  if (newStatus == status || newStatus == SPI_STATUS_FAILURE)
    return false;
  return true;
}

bool SPIRadioModule::send(const union RequestPacket *packet) {
  return sendGeneric(packet, sizeof(RequestPacket), SPI_STATUS_TRANSMITING);
}

bool SPIRadioModule::pair() {
  ResponsePacket resp;

  PRINTLN(F("SPI: Starting pairing"));
  writeStatus(SPI_STATUS_PAIRING);

  for (
      unsigned long start = millis();
      millis() - start < PAIR_TIMEOUT;
      delay(1)
  ) {
    if (!receiveGeneric(&resp, sizeof(ResponsePacket), SPI_STATUS_PAIRED))
      continue;
    if (resp.pair.packetType != PACKET_TYPE_PAIR)
      continue;
    if (resp.pair.status != PAIR_STATUS_READY)
      continue;
    PRINTLN(F("SPI: Paired"));
    memcpy(peer.address, resp.pair.sender.address, ADDRESS_LENGTH);
    writeStatus(SPI_STATUS_OK);
    return true;
  }

  PRINTLN(F("SPI: Not paired"));
  writeStatus(SPI_STATUS_OK);
  return false;
}

// vim:et:sw=2:ai
