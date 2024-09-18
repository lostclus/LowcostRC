#include <string.h>
#include <EEPROM.h>
#include <Servo.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_Rx_nRF24.h>

#define PAIR_PIN 2

#define CHANNEL1_PIN 3
#define CHANNEL2_PIN 4
#define CHANNEL3_PIN 5

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 10000L
#define VOLT_METER_R2 10000L

#define RANDOM_SEED_PIN A1

#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x1235

#define ENGINE_POWER_MIN 1000
#define ENGINE_POWER_MAX 2000

struct Settings {
  uint16_t magick;
  Address address;
  RFChannel rfChannel;
  PALevel paLevel;
  ControlPacket failsafe;
} settings;

const Settings defaultSettings PROGMEM = {
  SETTINGS_MAGICK,
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL,
  DEFAULT_PA_LEVEL,
  {
    PACKET_TYPE_CONTROL,
    {
      1000, 1500, 1500, 1500,
      1000, 1000, 1000, 1000
    }
  }
};

unsigned int battaryMV = 0;
unsigned long controlTime,
              sendTelemetryTime;
bool isFailsafe = false;

Servo channel2Servo,
      channel3Servo;
NRF24Receiver receiver(RADIO_CE_PIN, RADIO_CSN_PIN);

void setup(void) {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif

  pinMode(PAIR_PIN, INPUT_PULLUP);
  analogReference(DEFAULT);
  randomSeed(analogRead(RANDOM_SEED_PIN));

  pinMode(CHANNEL1_PIN, OUTPUT);
  digitalWrite(CHANNEL1_PIN, LOW);

  channel2Servo.attach(CHANNEL2_PIN);
  channel3Servo.attach(CHANNEL3_PIN);


  PRINTLN(F("Reading settings from flash ROM..."));
  EEPROM.get(SETTINGS_ADDR, settings);
  if (settings.magick != SETTINGS_MAGICK) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&settings, &defaultSettings, sizeof(Settings));
    for (int i = 0; i < NRF24_ADDRESS_LENGTH; i++)
      settings.address.address[i] = random(1 << 8);
    EEPROM.put(SETTINGS_ADDR, settings);
  }  else {
    PRINTLN(F("Using stored settings in flash ROM"));
  }

  receiver.begin(&settings.address, settings.rfChannel);
  receiver.setPALevel(settings.paLevel);

  controlTime = 0;
  sendTelemetryTime = 0;
}

void loop(void) {
  unsigned long now = millis();
  union RequestPacket rp;
  static int lastChannels[NUM_CHANNELS];
  static bool hasLastChannels = false;

  if (receiver.receive(&rp)) {
    if (rp.generic.packetType == PACKET_TYPE_CONTROL) {
      controlTime = now;
      isFailsafe = false;

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
    } else if (rp.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
      PRINT(F("New RF channel: "));
      PRINTLN(rp.rfChannel.rfChannel);
      receiver.setRFChannel(rp.rfChannel.rfChannel);
      settings.rfChannel = rp.rfChannel.rfChannel;
      EEPROM.put(SETTINGS_ADDR, settings);
    } else if (rp.generic.packetType == PACKET_TYPE_SET_PA_LEVEL) {
      PRINT(F("New PA level: "));
      PRINTLN(rp.paLevel.paLevel);
      receiver.setPALevel(rp.paLevel.paLevel);
      settings.paLevel = rp.paLevel.paLevel;
      EEPROM.put(SETTINGS_ADDR, settings);
    } else if (rp.generic.packetType == PACKET_TYPE_COMMAND) {
      if (rp.command.command == COMMAND_SAVE_FAILSAFE && hasLastChannels) {
        PRINT(F("Saving state for failsafe"));
        rp.control.packetType = PACKET_TYPE_CONTROL;
        for (int channel = 0; channel < NUM_CHANNELS; channel++)
          rp.control.channels[channel] = lastChannels[channel];
        memcpy(&settings.failsafe, &rp.control, sizeof(ControlPacket));
        EEPROM.put(SETTINGS_ADDR, settings);
      }
    }
  }

  if (now - sendTelemetryTime > 5000) {
    sendTelemetry();
    sendTelemetryTime = now;
  }


  if (!isFailsafe && controlTime > 0 && now - controlTime > 1250) {
    PRINTLN(F("Radio signal lost"));
    isFailsafe = true;
    applyControl(&settings.failsafe);
  }

  if (digitalRead(PAIR_PIN) == LOW) {
    receiver.pair();
  }
}

void applyControl(ControlPacket *control) {
  analogWrite(
    CHANNEL1_PIN,
    map(
      constrain(control->channels[CHANNEL1], ENGINE_POWER_MIN, ENGINE_POWER_MAX),
      ENGINE_POWER_MIN, ENGINE_POWER_MAX, 0, 255
    )
  );

  if (control->channels[CHANNEL2])
    channel2Servo.writeMicroseconds(control->channels[CHANNEL2]);
  if (control->channels[CHANNEL3])
    channel3Servo.writeMicroseconds(control->channels[CHANNEL3]);
}

void sendTelemetry() {
  ResponsePacket resp;

  resp.telemetry.packetType = PACKET_TYPE_TELEMETRY;

  updateBattaryVoltage();
  PRINT(F("battaryMV: "));
  PRINTLN(battaryMV);
  resp.telemetry.battaryMV = battaryMV;

  receiver.send(&resp);
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
