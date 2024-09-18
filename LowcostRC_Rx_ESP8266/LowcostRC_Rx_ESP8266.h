#ifndef LOWCOSTRC_RX_ESP8266_H
#define LOWCOSTRC_RX_ESP8266_H

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Rx.h>

class ESP8266Receiver : public BaseReceiver {
  private:
    Address address, peer;
    RFChannel rfChannel;
    RequestPacket request;
    uint8_t requestMac[6];
    unsigned long requestTime,
                  receiveTime;
    bool isPairing, _isPaired;

    uint8_t rfChannelToWifi(RFChannel ch);
    void ensurePeerExist(uint8_t *mac, uint8_t wifiChannel);
  public:

    ESP8266Receiver();
    bool begin(const Address *peer, RFChannel ch);

    virtual const Address *getAddress();
    virtual const Address *getPeerAddress();
    virtual RFChannel getRFChannel();
    virtual void setRFChannel(RFChannel ch);
    virtual void setPALevel(PALevel level);
    virtual bool receive(RequestPacket *packet);
    virtual void send(const ResponsePacket *packet);
    virtual bool pair();
    virtual bool isPaired();

    void _onDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len);
};

#endif // LOWCOSTRC_RX_ESP8266_H
// vim:et:sw=2:ai
