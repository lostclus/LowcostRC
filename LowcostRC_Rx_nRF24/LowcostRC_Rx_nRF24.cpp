#include <LowcostRC_Rx_nRF24.h>
#include <LowcostRC_Console.h>

NRF24Receiver::NRF24Receiver(uint8_t cepin, uint8_t cspin)
  : rf24(cepin, cspin),
    address(ADDRESS_NONE)
{
}

uint8_t NRF24Receiver::rfChannelToNRF24(RFChannel ch) {
  return (ch == 0) ? NRF24_DEFAULT_CHANNEL : ch;
}

void NRF24Receiver::configure(const Address *addr, RFChannel ch) {
#ifdef WITH_CONSOLE
  char text[18];
#endif

  rf24.closeReadingPipe(1);
  rf24.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  rf24.setPayloadSize(PACKET_SIZE);
  rf24.enableAckPayload();
  rf24.openReadingPipe(1, addr->address);
  rf24.setChannel(rfChannelToNRF24(ch));
  rf24.startListening();

#ifdef WITH_CONSOLE
  sprintf_P(
    text,
    PSTR("%02x:%02x:%02x:%02x:%02x:%02x"),
    addr->address[0],
    addr->address[1],
    addr->address[2],
    addr->address[3],
    addr->address[4],
    addr->address[5]
  );
  PRINT(F("Config: "));
  PRINT(F("addr: "));
  PRINT(text);
  PRINT(F("; RF channel: "));
  PRINT(ch);
  PRINT(F(": NRF24 channel: "));
  PRINTLN(rfChannelToNRF24(ch));
#endif
}

bool NRF24Receiver::begin(const Address *addr, RFChannel ch) {
  if (!rf24.begin()) {
    PRINTLN(F("NRF24: init: FAIL"));
    return false;
  }
  PRINTLN(F("NRF24: init: OK"));

  memcpy(address.address, addr->address, ADDRESS_LENGTH);
  rfChannel = ch;

  configure(&address, rfChannel);
  return true;
}

const Address *NRF24Receiver::getAddress() {
  return &address;
}

const Address *NRF24Receiver::getPeerAddress() {
  return &address;
}

RFChannel NRF24Receiver::getRFChannel() {
  return rfChannel;
}

void NRF24Receiver::setRFChannel(RFChannel ch) {
  rf24.setChannel(rfChannelToNRF24(ch));
  PRINT(F("RF channel: "));
  PRINTLN(ch);
  PRINT(F("NRF24 channel: "));
  PRINTLN(rfChannelToNRF24(ch));
}

bool NRF24Receiver::receive(RequestPacket *packet) {
  if (!rf24.available()) return false;
  rf24.read(packet, sizeof(RequestPacket));
  return true;
}

void NRF24Receiver::send(const ResponsePacket *packet) {
  rf24.writeAckPayload(1, packet, sizeof(ResponsePacket));
}

bool NRF24Receiver::pair() {
  Address broadcast = ADDRESS_BROADCAST;
  RequestPacket req;
  ResponsePacket resp;
  int readyCount = 0;

  PRINTLN(F("NRF24: Starting pairing"));
  rf24.stopListening();
  configure(&broadcast, NRF24_DEFAULT_CHANNEL);

  for (
    unsigned long start = millis();
    millis() - start < 10000;
  ) {
    if (rf24.available()) {
      rf24.read(&req, sizeof(RequestPacket));

      if (req.pair.packetType != PACKET_TYPE_PAIR) {
        PRINTLN("NRF24: Ignoring non-pair type packet");
        continue;
      }

      readyCount = 0;

      if (req.pair.status == PAIR_STATUS_INIT) {
        PRINT(F("NRF24: Ready to pair in session: "));
        PRINTLN(req.pair.session);
        resp.pair.packetType = PACKET_TYPE_PAIR;
        resp.pair.status = PAIR_STATUS_READY;
        resp.pair.session = req.pair.session;
        memcpy(resp.pair.sender.address, address.address, ADDRESS_LENGTH);
        readyCount = 10;
      } else if (req.pair.status == PAIR_STATUS_PAIRED) {
        PRINTLN(F("NRF24: Paired"));
        rf24.stopListening();
        configure(&address, NRF24_DEFAULT_CHANNEL);
        return true;
      }
    } else {
      delay(50);
    }
    if (readyCount) {
      readyCount--;
      PRINTLN("NRF24: Sending pair ready response");
      rf24.writeAckPayload(1, &resp, sizeof(ResponsePacket));
      delay(5);
    }
  };

  PRINTLN(F("NRF24: Not paired"));
  return false;
}

bool NRF24Receiver::isPaired() {
  return true;
}

// vim:ai:sw=2:et
