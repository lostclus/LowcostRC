#include <LowcostRC_Rx_nRF24.h>
#include <LowcostRC_Rx_Controller.h>
#include <LowcostRC_Rx_Settings.h>
#include <LowcostRC_Output.h>
#include <LowcostRC_VoltMetter.h>

#define PAIR_PIN 2

#define CHANNEL1_PIN 3
#define CHANNEL2_PIN 4
#define CHANNEL3_PIN 5

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 10000L
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
NRF24Receiver receiver(RADIO_CE_PIN, RADIO_CSN_PIN);
VoltMetter voltMetter(VOLT_METER_PIN, VOLT_METER_R1, VOLT_METER_R2);
RxController controller(&settings, &receiver, outputs, &voltMetter, PAIR_PIN);

void setup(void) {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  controller.begin();
}

void loop(void) {
  controller.handle();
}

// vim:ai:sw=2:et
