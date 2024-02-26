#ifndef Radio_Control_h
#define Radio_Control_h

#include <LowcostRC_Protocol.h>
#include "Radio.h"
#include "Radio_NRF24.h"
#include "Radio_SPI.h"
#include "Buzzer.h"
#include "Config.h"

class RadioControl {
  private:
#ifdef WITH_RADIO_NRF24
    NRF24RadioModule nrf24Radio;
#endif
#ifdef WITH_RADIO_SPI
    SPIRadioModule spiRadio;
#endif
    Buzzer *buzzer;
  public:
    BaseRadioModule *radio;
    struct TelemetryPacket telemetry;
    unsigned long requestSendTime = 0,
                  telemetryTime = 0,
                  errorTime = 0;
    bool statusRadioSuccess = false,
         statusRadioFailure = false;

    RadioControl(Buzzer *buzzer);
    void begin();
    void sendRFChannel(RFChannel channel);
    void sendCommand(Command command);
    void sendPacket(const union RequestPacket *packet);
    void handle();
};

#endif // Radio_Control_h
// vim:et:sw=2:ai
