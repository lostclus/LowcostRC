#ifndef LOWCOSTRC_RX_NRF24_H
#define LOWCOSTRC_RX_NRF24_H

#include <LowcostRC_Protocol.h>
#include <RF24.h>

#define NRF24_DEFAULT_CHANNEL 76
#define NRF24_ADDRESS_LENGTH 5

class NRF24Receiver {
  private:
    RF24 rf24;
    uint8_t rfChannelToNRF24(RFChannel ch);
    void configure(const Address *addr, RFChannel ch);
  public:
    Address address;

    NRF24Receiver(uint8_t cepin, uint8_t cspin);
    bool begin(const Address *addr, RFChannel ch);
    void setRFChannel(RFChannel ch);
    bool receive(RequestPacket *packet);
    void send(const ResponsePacket *packet);
    bool pair();
};

#endif // LOWCOSTRC_RX_NRF24_H
// vim:et:sw=2:ai
