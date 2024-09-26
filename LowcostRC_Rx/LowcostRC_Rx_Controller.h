#ifndef LOWCOSTRC_RX_CONTROLLER_H
#define LOWCOSTRC_RX_CONTROLLER_H

#include <LowcostRC_Protocol.h>
#include <LowcostRC_VoltMetter.h>
#include <LowcostRC_Rx.h>
#include <LowcostRC_Rx_Settings.h>
#include <LowcostRC_Output.h>

class RxController {
  private:
    uint16_t lastChannels[NUM_CHANNELS];
    bool hasLastChannels,
         isLedInverted;

  public:
    BaseRxSettings *settings;
    BaseReceiver *receiver;
    BaseOutput *outputs[NUM_CHANNELS];
    VoltMetter *voltMetter;

    int pairPin, ledPin;
    unsigned long controlTime, telemetryTime;
    bool isFailsafe;

    RxController(
        BaseRxSettings *settings,
        BaseReceiver *receiver,
        BaseOutput **outputs = NULL,
        VoltMetter *voltMetter = NULL,
        int pairPin = -1,
        int ledPin = -1
    );
    virtual bool begin();
    virtual void handle();
    virtual void handlePacket(const RequestPacket *rp);
    virtual void applyControl(const ControlPacket *control);
    virtual void sendTelemetry();
    void setLedInverted(bool value);
};

#endif // LOWCOSTRC_RX_CONTROLLER_H
// vim:et:sw=2:ai
