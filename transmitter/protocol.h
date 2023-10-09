const char DEFAULT_PIPE_ADDR[] = "lcrc00";
const size_t PACKET_SIZE = 16;

enum PacketType {
    PACKET_TYPE_CONTROL = 0x0a01,
    PACKET_TYPE_STATUS = 0x0a02,
    PACKET_TYPE_SET_PIPE_ADDRESS = 0x0a03
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
  char pipeAddress[7];
};

union RequestPacket {
    struct GenericPacket generic;
    struct ControlPacket control;
    struct SetAddressPacket address;
};
