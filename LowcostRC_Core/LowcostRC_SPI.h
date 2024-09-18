#ifndef LowcostRC_SPI_h
#define LowcostRC_SPI_h

#include <stdint.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Tx.h>

const size_t SPI_PACKET_SIZE = 32;

const uint32_t SPI_STATUS_OK = 0x0000;
const uint32_t SPI_STATUS_FAILURE = 0x0001;
const uint32_t SPI_STATUS_STARTING = 0x0011;
const uint32_t SPI_STATUS_TRANSMITING = 0x0012;
const uint32_t SPI_STATUS_RECEIVING = 0x0013;
const uint32_t SPI_STATUS_PAIRING = 0x0014;
const uint32_t SPI_STATUS_PAIRED = 0x0015;
const uint32_t SPI_STATUS_SET_PEER_ADDRESS = 0x0016;
const uint32_t SPI_STATUS_SET_CHANNEL = 0x0017;
const uint32_t SPI_STATUS_SET_PA_LEVEL = 0x0018;

enum SPIPacketTypeEnum {
  SPI_PACKET_TYPE_INIT = 0x0b01,
  SPI_PACKET_TYPE_SET_PEER_ADDRESS = 0x0b02,
  SPI_PACKET_TYPE_SET_RF_CHANNEL = 0x0b03,
  SPI_PACKET_TYPE_SET_PA_LEVEL = 0x0b04,
  SPI_PACKET_TYPE_PAIRING = 0x0b05,
};

typedef uint16_t SPIPacketType;

struct SPIInitPacket {
  SPIPacketType packetType;
  TxModuleType moduleType;
  uint8_t numRFChannels;
  uint8_t numPALevels;
} __attribute__((__packed__));

struct SPISetPeerAddressPacket {
  SPIPacketType packetType;
  Address peer;
} __attribute__((__packed__));

struct SPISetRFChannelPacket {
  SPIPacketType packetType;
  RFChannel rfChannel;
} __attribute__((__packed__));

struct SPISetPALevelPacket {
  SPIPacketType packetType;
  PALevel paLevel;
} __attribute__((__packed__));

union SPIRequestPacket {
  union RequestPacket request;
  struct SPISetPeerAddressPacket peerAddr;
  struct SPISetRFChannelPacket rfChannel;
  struct SPISetPALevelPacket paLevel;
};

union SPIResponsePacket {
  union ResponsePacket response;
  struct SPIInitPacket init;
};

#endif
// vim:et:sw=2:ai
