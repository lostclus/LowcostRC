#include <EEPROM.h>
#include <RF24.h>
#include <Servo.h>

#include "protocol.h"

#undef WITH_CONSOLE
//define WITH_CONSOLE

#define CHANNEL1_PIN 3
#define CHANNEL2_PIN 4
#define CHANNEL3_PIN 5

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 30000L
#define VOLT_METER_R2 10000L

#define PIPE_ADDRESS_ROM_ADDR 0
#define NOLINK_ROM_ADDR 7

const ControlPacket DEFAULT_NOLINK_CONTROL = {
  PACKET_TYPE_CONTROL,
  {1500, 1500, 1500, 1500}
};

unsigned int battaryMV = 0;
unsigned long controlTime,
              sendStatusTime;

Servo channel1Servo, channel2Servo, channel3Servo;
RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

void setup(void) {
  char pipeAddressBuf[7];
  char *pipeAddress;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif

  analogReference(DEFAULT);

  channel1Servo.attach(CHANNEL1_PIN);
  channel2Servo.attach(CHANNEL2_PIN);
  channel3Servo.attach(CHANNEL3_PIN);

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN(F("Radio init: FAILURE"));
  }
  radio.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  radio.setPayloadSize(PACKET_SIZE);

  EEPROM.get(PIPE_ADDRESS_ROM_ADDR, pipeAddressBuf);
  if (pipeAddressBuf[0] == DEFAULT_PIPE_ADDR[0]) {
    pipeAddress = pipeAddressBuf;
  } else {
    pipeAddress = DEFAULT_PIPE_ADDR;
  }

  radio.openReadingPipe(1, (byte*)pipeAddress);
  PRINT(F("Radio pipe address: "));
  PRINTLN(pipeAddress);

  radio.enableAckPayload();
  radio.startListening();

  controlTime = 0;
  sendStatusTime = 0;
}

void loop(void) {
  unsigned long now = millis();
  union RequestPacket rp;
  static int lastChannels[NUM_CHANNELS];
  static bool hasLastChannels = false;

  if (now - sendStatusTime > 5000) {
    sendStatus();
    sendStatusTime = now;
  }

  if (radio.available()) {
    radio.read(&rp, sizeof(rp));
    if (rp.generic.packetType == PACKET_TYPE_CONTROL) {
      controlTime = now;

      for (int channel = 0; channel < NUM_CHANNELS; channel++)
        lastChannels[channel] = rp.control.channels[channel];
      hasLastChannels = true;

      #ifdef WITH_CONSOLE
      if (now / 1000 % 2 == 0) {
        PRINT(F("ch1: "));
        PRINT(rp.control.channels[CHANNEL1]);
        PRINT(F(", "));
        PRINT(F("ch2: "));
        PRINT(rp.control.channels[CHANNEL2]);
        PRINT(F(", "));
        PRINT(F("ch3: "));
        PRINTLN(rp.control.channels[CHANNEL3]);
      }
      #endif

      applyControl(&rp.control);
    } else if (rp.generic.packetType == PACKET_TYPE_SET_PIPE_ADDRESS) {
      radio.openReadingPipe(1, (byte*)rp.address.pipeAddress);
      PRINT(F("New radio pipe address: "));
      PRINTLN(rp.address.pipeAddress);
      EEPROM.put(PIPE_ADDRESS_ROM_ADDR, rp.address.pipeAddress);
    } else if (rp.generic.packetType == PACKET_TYPE_COMMAND) {
      if (rp.command.command == COMMAND_SAVE_FOR_NOLINK && hasLastChannels) {
        PRINT(F("Saving state for no link"));
        rp.control.packetType = PACKET_TYPE_CONTROL;
        for (int channel = 0; channel < NUM_CHANNELS; channel++)
          rp.control.channels[channel] = lastChannels[channel];
        EEPROM.put(NOLINK_ROM_ADDR, rp.control);
      }
    }
  }

  if (controlTime > 0 && now - controlTime > 1250) {
    PRINTLN(F("Radio signal lost"));
    EEPROM.get(NOLINK_ROM_ADDR, rp.control);

    if (rp.control.packetType == PACKET_TYPE_CONTROL) {
      applyControl(&rp.control);
    } else {
      applyControl(&DEFAULT_NOLINK_CONTROL);
    }
  }
}

void applyControl(ControlPacket *control) {
  if (control->channels[CHANNEL1])
    channel1Servo.writeMicroseconds(control->channels[CHANNEL1]);
  if (control->channels[CHANNEL2])
    channel2Servo.writeMicroseconds(control->channels[CHANNEL2]);
  if (control->channels[CHANNEL3])
    channel3Servo.writeMicroseconds(control->channels[CHANNEL3]);
}

void sendStatus() {
  struct StatusPacket status;

  status.packetType = PACKET_TYPE_STATUS;

  updateBattaryVoltage();
  PRINT(F("battaryMV: "));
  PRINTLN(battaryMV);
  status.battaryMV = battaryMV;

  radio.writeAckPayload(1, &status, sizeof(status));
}

unsigned long vHist[5] = {0, 0, 0, 0, 0};
byte vHistPos = 0;

void updateBattaryVoltage() {
  byte i, count = 0;
  unsigned long vcc = 0, vpin, vsum = 0;
   
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
      ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
      ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
      ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
      // works on an Arduino 168 or 328
      ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  delay(3); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  // 1.1 * 1023 * 1000 = 1125300
  vcc = 1125300L / ((unsigned long)((high<<8) | low));
  vpin = analogRead(VOLT_METER_PIN);

  /*
  PRINT(F("vcc: "));
  PRINTLN(vcc);

  PRINT(F("vpin: "));
  PRINTLN(vpin);
  */

  vHist[vHistPos] = vpin * vcc;
  vHistPos = (vHistPos + 1) % (sizeof(vHist) / sizeof(vHist[0]));

  for (i = 0; i < sizeof(vHist) / sizeof(vHist[0]); i++) {
    if (vHist[i] > 0) {
      vsum += vHist[i];
      count += 1;
    }
  }

  battaryMV = (vsum / count) / 1024 
    * (1000L / (VOLT_METER_R2 * 1000L / (VOLT_METER_R1 + VOLT_METER_R2)));
}

// vim:ai:sw=2:et
