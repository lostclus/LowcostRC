#ifndef Radio_Control_h
#define Radio_Control_h

#include <LowcostRC_Protocol.h>
#include "Radio.h"
#include "Radio_NRF24.h"
#include "Radio_SPI.h"
#include "Buzzer.h"

class RadioControl {
  private:
    NRF24RadioModule nrf24Radio;
    SPIRadioModule spiRadio;
    Buzzer *buzzer;
  public:
    BaseRadioModule *radio;
    unsigned long requestSendTime = 0,
                  errorTime = 0;
    bool statusRadioSuccess = false,
         statusRadioFailure = false;

    RadioControl(Buzzer *buzzer);
    void begin();
    void sendRFChannel(int rfChannel);
    void sendCommand(Command command);
    void sendPacket(union RequestPacket *packet);
    void handle();
};

#endif // Radio_Control_h
// vim:et:sw=2:ai
