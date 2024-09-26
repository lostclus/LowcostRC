#include <LowcostRC_Rx_nRF24.h>
#include <LowcostRC_Rx_Controller.h>
#include <LowcostRC_Output.h>
#include <LowcostRC_VoltMetter.h>

// Pin that connected to the bind button
#define PAIR_PIN 2

// Pin that connected to the motor ESC
#define CHANNEL1_PIN 3

// Pin that connected to the alerons servo
#define CHANNEL2_PIN 4

// Pin that connected to that elevator servo
#define CHANNEL3_PIN 5

// Pin that connected to NRF24 CE pin
#define RADIO_CE_PIN 9

// Pin that connected to NRF24 CSN pin
#define RADIO_CSN_PIN 10

// Pin that connected to the resistor divider to measure battery voltage
#define VOLT_METER_PIN A0

// R1 resistor value in Ohm's (plus side)
#define VOLT_METER_R1 30000L

// R2 resistor value in Ohm's (minus side)
#define VOLT_METER_R2 10000L

// Use PWMMicrosecondsOutput to take exactly impulse width in microseconds that
// corresponds received value (1000..2000 by default). Suitable to control
// servo motors or ESCs.
PWMMicrosecondsOutput channel1Output(CHANNEL1_PIN),
                      channel2Output(CHANNEL2_PIN),
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
