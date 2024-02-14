#ifndef LowcostRC_Protocol_h
#define LowcostRC_Protocol_h

#include <stdint.h>

const int DEFAULT_RF_CHANNEL = 76;
const size_t PACKET_SIZE = 16;

#define PIPE_FORMAT "lcr%03d"

enum PacketTypeEnum {
  PACKET_TYPE_CONTROL = 0x0a01,
  PACKET_TYPE_TELEMETRY = 0x0a02,
  PACKET_TYPE_SET_RF_CHANNEL = 0x0a03,
  PACKET_TYPE_COMMAND = 0x0a04
};

typedef uint16_t PacketType;

struct GenericPacket {
  PacketType packetType;
};

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

typedef uint16_t ChannelN;

enum CommandEnum {
  COMMAND_SAVE_FOR_NOLINK,
  COMMAND_USER_COMMAND1,
  COMMAND_USER_COMMAND2
};

typedef uint16_t Command;

struct ControlPacket {
  PacketType packetType;
  uint16_t channels[NUM_CHANNELS];
};

struct TelemetryPacket{
  PacketType packetType;
  uint16_t battaryMV;
};

struct SetRFChannelPacket{
  PacketType packetType;
  uint16_t rfChannel;
};

struct CommandPacket{
  PacketType packetType;
  Command command;
};

union RequestPacket {
  struct GenericPacket generic;
  struct ControlPacket control;
  struct SetRFChannelPacket rfChannel;
  struct CommandPacket command;
};

#endif // LowcostRC_Protocol_h
// vim:ai:sw=2:et
