#ifndef Radio_SPI_h
#define Radio_SPI_h

#include <SPI.h>
#include "Radio.h"

class SPIRadioModule : public BaseRadioModule {
  private:
    void pulseSS();
    uint32_t readStatus();
    void writeStatus(uint32_t status);
    void readData(uint8_t *data);
    void writeData(uint8_t *data, size_t len);
  public:
    SPIRadioModule();
    virtual bool begin();
    virtual bool setRFChannel(int rfChannel);
    virtual bool receive(struct TelemetryPacket *telemetry);
    virtual bool send(union RequestPacket *packet);
};

#endif	//Radio_SPI_h
// vim:et:sw=2:ai
