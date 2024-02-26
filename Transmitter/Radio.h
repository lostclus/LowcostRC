#ifndef Radio_h
#define Radio_h

#include <LowcostRC_Protocol.h>

class BaseRadioModule {
  public:
    Address peer;
    RFChannel rfChannel;

    BaseRadioModule();
    virtual bool begin() = 0;
    virtual bool setPeer(const Address *addr) = 0;
    virtual bool setRFChannel(RFChannel ch) = 0;
    virtual bool receive(union ResponsePacket *packet) = 0;
    virtual bool send(const union RequestPacket *packet) = 0;
    virtual bool pair() = 0;

    bool isPaired();
    void unpair();
};

#endif // Radio_h
// vim:et:sw=2:ai
