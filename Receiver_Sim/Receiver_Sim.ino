#include <Joystick.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Rx_Settings.h>
#include <LowcostRC_Rx_nRF24.h>
#include <LowcostRC_Rx_Controller.h>

#define PAIR_PIN 2

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

class SimRxController : public RxController {
  public:
    Joystick_ Joystick;

    SimRxController(BaseRxSettings *settings, BaseReceiver *receiver, int pairPin)
      : RxController(settings, receiver, NULL, NULL, pairPin, -1)
      ,  Joystick(
        JOYSTICK_DEFAULT_REPORT_ID,
        JOYSTICK_TYPE_MULTI_AXIS,
        0, 0,
        true, // X
        true, // Y
        true, // Z
        true, // Rx
        true, // Ry
        true, // Rz
        true, // Rubber
        true, // Throttle
        false, // Accelerator
        false, // Brake
        false // Steering
      )
    {}

    virtual bool begin() {
      if (!RxController::begin())
        return false;

      Joystick.begin(false);
      Joystick.setXAxisRange(1000, 2000);
      Joystick.setYAxisRange(1000, 2000);
      Joystick.setZAxisRange(1000, 2000);
      Joystick.setRxAxisRange(1000, 2000);
      Joystick.setRyAxisRange(1000, 2000);
      Joystick.setRzAxisRange(1000, 2000);
      Joystick.setRudderRange(1000, 2000);
      Joystick.setThrottleRange(1000, 2000);
      return true;
    }

    virtual void applyControl(const ControlPacket *control) {
      Joystick.setZAxis(control->channels[CHANNEL1]);
      Joystick.setRxAxis(control->channels[CHANNEL2]);
      Joystick.setXAxis(control->channels[CHANNEL3]);
      Joystick.setYAxis(control->channels[CHANNEL4]);
      Joystick.setRyAxis(control->channels[CHANNEL5]);
      Joystick.setRzAxis(control->channels[CHANNEL6]);
      Joystick.setRudder(control->channels[CHANNEL7]);
      Joystick.setThrottle(control->channels[CHANNEL8]);
      Joystick.sendState();
    }
};

EEPROMRxSettings settings;
NRF24Receiver receiver(RADIO_CE_PIN, RADIO_CSN_PIN);
SimRxController controller(&settings, &receiver, PAIR_PIN);

void setup(void) {
#ifdef WITH_CONSOLE
  delay(3000);
  Serial.begin(115200);
#endif
  controller.begin();
}

void loop(void) {
  controller.handle();
}

// vim:ai:sw=2:et
