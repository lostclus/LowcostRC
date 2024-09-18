#ifndef Radio_NRF24_h
#define Radio_NRF24_h

#include <RF24.h>
#include "Radio.h"

class NRF24RadioModule : public BaseRadioModule {
  private:
    RF24 rf24;
    uint8_t rfChannelToNRF24(RFChannel ch);
  public:
    NRF24RadioModule();
    virtual bool begin();
    virtual TxModuleType getModuleType();
    virtual int getNumRFChannels();
    virtual int getNumPALevels();
    virtual bool setPeer(const Address *addr);
    virtual bool setRFChannel(RFChannel ch);
    virtual bool setPALevel(PALevel level);
    virtual bool receive(union ResponsePacket *telemetry);
    virtual bool send(const union RequestPacket *packet);
    virtual bool pair();
};

#endif	//Radio_NRF24_h
// vim:et:sw=2:ai
