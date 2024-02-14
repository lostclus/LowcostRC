#ifndef Radio_h
#define Radio_h

#include <LowcostRC_Protocol.h>

class BaseRadioModule {
  public:
    BaseRadioModule() {};
    virtual bool begin() = 0;
    virtual bool setRFChannel(int rfChannel) = 0;
    virtual bool receive(struct TelemetryPacket *telemetry) = 0;
    virtual bool send(union RequestPacket *packet) = 0;
};

#endif // Radio_h
// vim:et:sw=2:ai
