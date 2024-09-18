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
    Buzzer *buzzer;
    byte packetsFailureCount = 0,
        packetsCount = 0,
        prevLinkQuality = 0;
  public:
    BaseRadioModule *radio;
    struct TelemetryPacket telemetry;
    unsigned long requestSendTime = 0,
                  telemetryTime = 0,
                  errorTime = 0;
    byte linkQuality = 0;

    RadioControl(Buzzer *buzzer);
    void begin();
    void sendRFChannel(RFChannel channel);
    void sendPALevel(PALevel level);
    void sendCommand(Command command);
    void sendPacket(const union RequestPacket *packet);
    void handle();
};

#endif // Radio_Control_h
// vim:et:sw=2:ai
