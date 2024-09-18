#ifndef Radio_SPI_h
#define Radio_SPI_h

#include <SPI.h>
#include "Radio.h"

class SPIRadioModule : public BaseRadioModule {
  private:
    TxModuleType moduleType;
    uint8_t numRFChannels,
            numPALevels;

    void pulseSS();
    uint32_t readStatus();
    void writeStatus(uint32_t status);
    void readData(uint8_t *data);
    void writeData(uint8_t *data, size_t len);
    bool receiveGeneric(void *data, size_t size, uint32_t status);
    bool sendGeneric(const void *data, size_t size, uint32_t status);
  public:
    SPIRadioModule();
    virtual bool begin();
    virtual TxModuleType getModuleType();
    virtual int getNumRFChannels();
    virtual int getNumPALevels();
    virtual bool setPeer(const Address *addr);
    virtual bool setRFChannel(RFChannel ch);
    virtual bool setPALevel(PALevel level);
    virtual bool receive(union ResponsePacket *packet);
    virtual bool send(const union RequestPacket *packet);
    virtual bool pair();
};

#endif	//Radio_SPI_h
// vim:et:sw=2:ai
