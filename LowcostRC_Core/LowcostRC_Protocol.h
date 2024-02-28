#ifndef LowcostRC_Protocol_h
#define LowcostRC_Protocol_h

#include <Arduino.h>

const size_t PACKET_SIZE = 16;

#define ADDRESS_LENGTH 6
#define ADDRESS_NONE {{0, 0, 0, 0, 0, 0}}
#define ADDRESS_BROADCAST {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}
#define DEFAULT_RF_CHANNEL 0

enum PacketTypeEnum {
  PACKET_TYPE_CONTROL = 0x0a01,
  PACKET_TYPE_TELEMETRY = 0x0a02,
  PACKET_TYPE_SET_RF_CHANNEL = 0x0a03,
  PACKET_TYPE_COMMAND = 0x0a04,
  PACKET_TYPE_PAIR = 0x0a05,
};

typedef uint16_t PacketType;

enum ChannelNEnum {
  CHANNEL1,
  CHANNEL2,
  CHANNEL3,
  CHANNEL4,
  CHANNEL5,
  CHANNEL6,
  NUM_CHANNELS,
  NO_CHANNEL = -1
};

typedef int16_t ChannelN;

enum CommandEnum {
  COMMAND_SAVE_FOR_NOLINK,
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
};

typedef uint8_t RFChannel;

struct GenericPacket {
  PacketType packetType;
};

struct ControlPacket {
  PacketType packetType;
  uint16_t channels[NUM_CHANNELS];
};

struct TelemetryPacket {
  PacketType packetType;
  uint16_t battaryMV;
};

struct SetRFChannelPacket {
  PacketType packetType;
  RFChannel rfChannel;
};

struct CommandPacket {
  PacketType packetType;
  Command command;
};

struct PairPacket {
  PacketType packetType;
  PairStatus status;
  uint16_t session;
  Address sender;
};

union RequestPacket {
  struct GenericPacket generic;
  struct ControlPacket control;
  struct SetRFChannelPacket rfChannel;
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
