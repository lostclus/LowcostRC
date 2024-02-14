#include <string.h>
#include <SPI.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_SPI.h>

#include "Radio_SPI.h"

const int SS_PIN = 10;


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
  pinMode(SS_PIN, OUTPUT);
  SPI.begin();

  writeStatus(STATUS_STARTING);
  if (readStatus() != STATUS_STARTING) {
    PRINTLN(F("SPI: Init: FAIL"));
    return false;
  }
  PRINTLN(F("SPI: Init: OK"));
  return true;
}

bool SPIRadioModule::setRFChannel(int rfChannel) {
  writeStatus(STATUS_SET_RF_CHANNEL + rfChannel);
  PRINT(F("SPI: RF channel: "));
  PRINTLN(rfChannel);
  return true;
}

bool SPIRadioModule::receive(struct TelemetryPacket *telemetry) {
  uint8_t buf[SPI_PACKET_SIZE];
  unsigned long now = millis();
  static unsigned long telemetryTime = 0;

  if (now - telemetryTime < 5000) return false;

  memset(buf, 0, sizeof(buf));
  readData(buf);

  if (((TelemetryPacket*)buf)->packetType == PACKET_TYPE_TELEMETRY) {
    memcpy(telemetry, buf, sizeof(TelemetryPacket));
    telemetryTime = now;
    return true;
  }

  return false;
}

bool SPIRadioModule::send(union RequestPacket *packet) {
  uint32_t newStatus;

  writeStatus(STATUS_TRANSMITING);

  for (int i = 0; i < 5; i++) {
    unsigned long txTime = millis();
    writeData((uint8_t*)packet, sizeof(RequestPacket));
    while ((newStatus = readStatus()) == STATUS_TRANSMITING && millis() - txTime < 20) {
      delay(1);
    }
    if (newStatus == STATUS_OK) break;
  }

  switch (newStatus) {
    case STATUS_TRANSMITING:
    case STATUS_FAILURE:
      return false;
  }
  return true;
}

// vim:et:sw=2:ai
