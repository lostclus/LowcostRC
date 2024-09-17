#include <ESP_EEPROM.h>
#include <Servo.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_Rx_ESP8266.h>

#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x5502
#define PAIR_PIN 0
#define CHANNEL1_PIN 4
#define CHANNEL2_PIN 12
#define CHANNEL3_PIN 14

#define ENGINE_POWER_MIN 1000
#define ENGINE_POWER_MAX 2000

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 51100L
#define VOLT_METER_R2 10000L

struct Settings {
  uint16_t magick;
  Address peer;
  RFChannel rfChannel;
  ControlPacket failsafe;
} settings;

const struct Settings defaultSettings = {
  SETTINGS_MAGICK,
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL,
  {
    PACKET_TYPE_CONTROL,
    {
      0, 1500, 1500, 1500,
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
ESP8266Receiver receiver;

bool loadSettings() {
  if (EEPROM.percentUsed() >= 0) {
    EEPROM.get(SETTINGS_ADDR, settings);
    if (settings.magick == SETTINGS_MAGICK) {
      PRINTLN("Using stored settings in flash ROM");
      return true;
    }
  }
  PRINTLN(F("No stored settings found, use defaults"));
  memcpy(&settings, &defaultSettings, sizeof(settings));
  return false;
}

void saveSettings() {
  EEPROM.put(SETTINGS_ADDR, settings);
  EEPROM.commit();
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

void updateBattaryVoltage() {
  static unsigned long vHist[5] = {0, 0, 0, 0, 0};
  static byte vHistPos = 0;

  byte i, count = 0;
  unsigned long vpin, vsum = 0;

  vpin = analogRead(VOLT_METER_PIN);

  PRINT(F("vpin: "));
  PRINTLN(vpin);

  vHist[vHistPos] = vpin;
  vHistPos = (vHistPos + 1) % (sizeof(vHist) / sizeof(vHist[0]));

  for (i = 0; i < sizeof(vHist) / sizeof(vHist[0]); i++) {
    if (vHist[i] > 0) {
      vsum += vHist[i];
      count += 1;
    }
  }

  battaryMV = map(
      vsum / count,
      0, 1023,
      0, 1000 * (1000L / (VOLT_METER_R2 * 1000L / (VOLT_METER_R1 + VOLT_METER_R2)))
  );
}

void setup() {
#ifdef WITH_CONSOLE
  Serial.begin(115200);
#endif

  pinMode(PAIR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(CHANNEL1_PIN, OUTPUT);
  digitalWrite(CHANNEL1_PIN, LOW);
  channel2Servo.attach(CHANNEL2_PIN);
  channel3Servo.attach(CHANNEL3_PIN);

  EEPROM.begin(sizeof(Settings));
  loadSettings();

  receiver.begin(&settings.peer, settings.rfChannel);

  controlTime = 0;
  sendTelemetryTime = 0;
}

void loop() {
  unsigned long now = millis();
  union RequestPacket rp;
  static int lastChannels[NUM_CHANNELS];
  static bool hasLastChannels = false;

  if (receiver.receive(&rp)) {
    if (rp.generic.packetType == PACKET_TYPE_CONTROL) {
      digitalWrite(LED_BUILTIN, LOW);

      controlTime = now;
      isFailsafe = false;

      for (int channel = 0; channel < NUM_CHANNELS; channel++)
        lastChannels[channel] = rp.control.channels[channel];
      hasLastChannels = true;

      for (int channel = 0; channel < NUM_CHANNELS; channel++) {
        PRINT(F("ch"));
        PRINT(channel + 1);
        PRINT(F(": "));
        PRINT(rp.control.channels[channel]);
        if (channel < NUM_CHANNELS - 1) {
          PRINT(F(", "));
        } else {
          PRINTLN();
        }
      }

      applyControl(&rp.control);

      digitalWrite(LED_BUILTIN, HIGH);
    } else if (rp.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
      PRINT(F("New RF channel: "));
      PRINTLN(rp.rfChannel.rfChannel);
      receiver.setRFChannel(rp.rfChannel.rfChannel);
      settings.rfChannel = rp.rfChannel.rfChannel;
      saveSettings();
    } else if (rp.generic.packetType == PACKET_TYPE_COMMAND) {
      if (rp.command.command == COMMAND_SAVE_FAILSAFE && hasLastChannels) {
        PRINT(F("Saving state for failsafe"));
        rp.control.packetType = PACKET_TYPE_CONTROL;
        for (int channel = 0; channel < NUM_CHANNELS; channel++)
          rp.control.channels[channel] = lastChannels[channel];
        memcpy(&settings.failsafe, &rp.control, sizeof(ControlPacket));
        saveSettings();
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
    if (receiver.pair()) {
      memcpy(settings.peer.address, receiver.getPeerAddress()->address, ADDRESS_LENGTH);
      saveSettings();
    }
  }
}

// vim:et:ai:sw=2
