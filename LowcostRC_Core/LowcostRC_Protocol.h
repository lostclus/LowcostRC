#ifndef LowcostRC_Protocol_h
#define LowcostRC_Protocol_h

#include <Arduino.h>

const size_t PACKET_SIZE = 18;

#define ADDRESS_LENGTH 6
#define ADDRESS_NONE {{0, 0, 0, 0, 0, 0}}
#define ADDRESS_BROADCAST {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}
#define DEFAULT_RF_CHANNEL 0
#define DEFAULT_PA_LEVEL 0

enum PacketTypeEnum {
  PACKET_TYPE_CONTROL = 0x0a01,
  PACKET_TYPE_TELEMETRY = 0x0a02,
  PACKET_TYPE_SET_RF_CHANNEL = 0x0a03,
  PACKET_TYPE_SET_PA_LEVEL = 0x0a04,
  PACKET_TYPE_PAIR = 0x0a05,
  PACKET_TYPE_COMMAND = 0x0a06,
};

typedef uint16_t PacketType;

enum ChannelNEnum {
  CHANNEL1,
  CHANNEL2,
  CHANNEL3,
  CHANNEL4,
  CHANNEL5,
  CHANNEL6,
  CHANNEL7,
  CHANNEL8,
  NUM_CHANNELS,
  NO_CHANNEL = -1
};

typedef int16_t ChannelN;

enum CommandEnum {
  COMMAND_SAVE_FAILSAFE,
  COMMAND_USER_COMMAND1,
  COMMAND_USER_COMMAND2,
};

typedef uint16_t Command;

enum PairStatusEnum {
  PAIR_STATUS_INIT,
  PAIR_STATUS_READY,
  PAIR_STATUS_PAIRED,
};

typedef uint8_t PairStatus;

struct Address {
  uint8_t address[ADDRESS_LENGTH];
} __attribute__((__packed__));

typedef uint8_t RFChannel;
typedef uint8_t PALevel;

struct GenericPacket {
  PacketType packetType;
} __attribute__((__packed__));

struct ControlPacket {
  PacketType packetType;
  uint16_t channels[NUM_CHANNELS];
} __attribute__((__packed__));

struct TelemetryPacket {
  PacketType packetType;
  uint16_t batteryMV;
} __attribute__((__packed__));

struct SetRFChannelPacket {
  PacketType packetType;
  RFChannel rfChannel;
} __attribute__((__packed__));

struct SetPALevelPacket {
  PacketType packetType;
  PALevel paLevel;
} __attribute__((__packed__));

struct CommandPacket {
  PacketType packetType;
  Command command;
} __attribute__((__packed__));

struct PairPacket {
  PacketType packetType;
  PairStatus status;
  uint16_t session;
  Address sender;
} __attribute__((__packed__));

union RequestPacket {
  struct GenericPacket generic;
  struct ControlPacket control;
  struct SetRFChannelPacket rfChannel;
  struct SetPALevelPacket paLevel;
  struct CommandPacket command;
  struct PairPacket pair;
};

union ResponsePacket {
  struct GenericPacket generic;
  struct TelemetryPacket telemetry;
  struct PairPacket pair;
};

#endif // LowcostRC_Protocol_h
// vim:ai:sw=2:et
