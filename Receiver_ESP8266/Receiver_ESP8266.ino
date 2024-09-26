#include <LowcostRC_Rx_ESP8266.h>
#include <LowcostRC_Rx_Controller.h>
#include <LowcostRC_Rx_Settings.h>
#include <LowcostRC_Output.h>
#include <LowcostRC_VoltMetter.h>

#define PAIR_PIN 0

#define CHANNEL1_PIN 4
#define CHANNEL2_PIN 12
#define CHANNEL3_PIN 14

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 51100L
#define VOLT_METER_R2 10000L

PWMDutyCycleOutput channel1Output(CHANNEL1_PIN);
PWMMicrosecondsOutput channel2Output(CHANNEL2_PIN),
                      channel3Output(CHANNEL3_PIN);

BaseOutput *outputs[] = {
  &channel1Output,
  &channel2Output,
  &channel3Output,
  NULL
};

EEPROMRxSettings settings;
ESP8266Receiver receiver;
VoltMetter voltMetter(VOLT_METER_PIN, VOLT_METER_R1, VOLT_METER_R2);
RxController controller(&settings, &receiver, outputs, &voltMetter, PAIR_PIN, LED_BUILTIN);

void setup() {
#ifdef WITH_CONSOLE
  Serial.begin(115200);
#endif

  controller.setLedInverted(true);
  controller.begin();
}

void loop() {
  controller.handle();
}

// vim:et:ai:sw=2
