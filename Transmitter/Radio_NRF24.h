#ifndef Radio_NRF24_h
#define Radio_NRF24_h

#include <RF24.h>
#include "Radio.h"

class NRF24RadioModule : public BaseRadioModule {
  private:
    RF24 rf24;
  public:
    NRF24RadioModule();
    virtual bool begin();
    virtual bool setRFChannel(int rfChannel);
    virtual bool receive(struct TelemetryPacket *telemetry);
    virtual bool send(union RequestPacket *packet);
};

#endif	//Radio_NRF24_h
// vim:et:sw=2:ai
