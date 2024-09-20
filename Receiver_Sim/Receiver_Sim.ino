#include <EEPROM.h>
#include <Joystick.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_Rx_nRF24.h>

#define PAIR_PIN 2

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define RANDOM_SEED_PIN A1

#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x1235

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
      1500, 1500, 1500, 1500,
      1000, 1000, 1000, 1000
    }
  }
};


unsigned long controlTime,
              sendTelemetryTime;
bool isFailsafe = false;

NRF24Receiver receiver(RADIO_CE_PIN, RADIO_CSN_PIN);

Joystick_ Joystick(
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
);

void setup(void) {
#ifdef WITH_CONSOLE
  delay(2000);
  Serial.begin(115200);
#endif

  pinMode(PAIR_PIN, INPUT_PULLUP);
  randomSeed(analogRead(RANDOM_SEED_PIN));

  Joystick.begin(false);
  Joystick.setXAxisRange(1000, 2000);
  Joystick.setYAxisRange(1000, 2000);
  Joystick.setZAxisRange(1000, 2000);
  Joystick.setRxAxisRange(1000, 2000);
  Joystick.setRyAxisRange(1000, 2000);
  Joystick.setRzAxisRange(1000, 2000);
  Joystick.setRudderRange(1000, 2000);
  Joystick.setThrottleRange(1000, 2000);

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
        PRINT(rp.control.channels[CHANNEL3]);
        PRINT(F(", "));
        PRINT(F("ch4: "));
        PRINT(rp.control.channels[CHANNEL4]);
        PRINT(F(", "));
        PRINT(F("ch5: "));
        PRINT(rp.control.channels[CHANNEL5]);
        PRINT(F(", "));
        PRINT(F("ch6: "));
        PRINT(rp.control.channels[CHANNEL6]);
        PRINT(F(", "));
        PRINT(F("ch7: "));
        PRINT(rp.control.channels[CHANNEL7]);
        PRINT(F(", "));
        PRINT(F("ch8: "));
        PRINTLN(rp.control.channels[CHANNEL7]);
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

void sendTelemetry() {
  ResponsePacket resp;

  resp.telemetry.packetType = PACKET_TYPE_TELEMETRY;
  resp.telemetry.batteryMV = 5000;

  receiver.send(&resp);
}

// vim:ai:sw=2:et
