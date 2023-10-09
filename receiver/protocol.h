const char DEFAULT_PIPE_ADDR[] = "lcrc0";
const size_t PACKET_SIZE = 16;

enum PacketType {
  PACKET_TYPE_CONTROL = 0x0a01,
  PACKET_TYPE_STATUS = 0x0a02,
  PACKET_TYPE_SET_PIPE_ADDRESS = 0x0a03,
  PACKET_TYPE_COMMAND = 0x0a04
};

struct GenericPacket {
  PacketType packetType;
};

enum ChannelN {
  CHANNEL1,
  CHANNEL2,
  CHANNEL3,
  CHANNEL4,
  NUM_CHANNELS,
  NO_CHANNEL = -1
};

enum Command {
  COMMAND_SAVE_FOR_NOLINK,
  COMMAND_USER_COMMAND1,
  COMMAND_USER_COMMAND2
};

struct ControlPacket {
  PacketType packetType;
  int channels[NUM_CHANNELS];
};

struct StatusPacket{
  PacketType packetType;
  int battaryMV;
};

struct SetAddressPacket{
  PacketType packetType;
  char pipeAddress[6];
};

struct CommandPacket{
  PacketType packetType;
  Command command;
};

union RequestPacket {
  struct GenericPacket generic;
  struct ControlPacket control;
  struct SetAddressPacket address;
  struct CommandPacket command;
};

// vim:ai:sw=2:et
