#include <string.h>
#include <ESP_EEPROM.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0
#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x5501
#define PAIR_PIN 0
#define CHANNEL1_PIN 4
#define CHANNEL2_PIN 12
#define CHANNEL3_PIN 14

#define ENGINE_POWER_MIN 1000
#define ENGINE_POWER_MAX 2000

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 51100L
#define VOLT_METER_R2 10000L

Servo channel2Servo,
      channel3Servo;

RequestPacket request;
ResponsePacket response;
unsigned long requestTime,
              processTime,
              sendTelemetryTime;
uint8_t requestMac[6];
bool isPairing = false,
     isFailsafe = false;
unsigned int battaryMV = 0;

struct Settings {
  uint16_t magick;
  bool isPaired;
  Address peer;
  RFChannel rfChannel;
  ControlPacket failsafe;
} settings;

const struct Settings defaultSettings = {
  SETTINGS_MAGICK,
  false,
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL,
  {
    PACKET_TYPE_CONTROL,
    {0, 1500, 1500, 1500}
  }
};

uint8_t rfChannelToWifi(RFChannel ch) {
  uint8_t wch = ch % 11;
  return wch == 0 ? 11 : wch;
}

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

void ensurePeerExist(uint8_t *mac, uint8_t wifiChannel) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, wifiChannel, NULL, 0);
  } else {
    esp_now_set_peer_channel(mac, wifiChannel);
  }
}

void sendPairReadyResponse(uint8_t *mac, uint16_t session) {
  response.pair.packetType = PACKET_TYPE_PAIR;
  response.pair.status = PAIR_STATUS_READY;
  response.pair.session = session;
  WiFi.macAddress(response.pair.sender.address);

  PRINTLN("Sending pair ready response");
  if (esp_now_send(mac, (uint8_t*)&response, sizeof(response)) != ESP_OK) {
    PRINTLN("Error sending pair ready response");
  }
}

void sendTelemetry(uint8_t *mac) {
  response.telemetry.packetType = PACKET_TYPE_TELEMETRY;

  updateBattaryVoltage();

  response.telemetry.battaryMV = battaryMV;
  if (esp_now_send(mac, (uint8_t*)&response, sizeof(response)) != ESP_OK) {
    PRINTLN("Error sending telemetry");
  }
}

void OnDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len) {
  if (len < sizeof(RequestPacket)) {
    PRINT("Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  memcpy(&request, incomingData, sizeof(RequestPacket));
  memcpy(requestMac, mac, sizeof(requestMac));
  requestTime = millis();
  isFailsafe = false;
}

void processRequest() {
  unsigned long now = millis();
  static int lastChannels[NUM_CHANNELS];
  static bool hasLastChannels = false;

  processTime = now;

  if (!settings.isPaired) {
    if (isPairing && request.pair.packetType == PACKET_TYPE_PAIR) {
      if (request.pair.status == PAIR_STATUS_INIT) {
        PRINTLN("Recived pair init packet");
        ensurePeerExist(requestMac, rfChannelToWifi(DEFAULT_RF_CHANNEL));
        sendPairReadyResponse(requestMac, request.pair.session);
        return;
      } else if (request.pair.status == PAIR_STATUS_PAIRED) {
        PRINTLN("Paired");
              isPairing = false;
        settings.isPaired = true;
        memcpy(settings.peer.address, requestMac, ADDRESS_LENGTH);
        ensurePeerExist(requestMac, rfChannelToWifi(DEFAULT_RF_CHANNEL));
        settings.rfChannel = DEFAULT_RF_CHANNEL;
        saveSettings();
        sendTelemetry(requestMac);
        return;
      }
    }
    PRINTLN("Ignoring packet in unpaired state");
    return;
  } else {
    if (memcmp(requestMac, settings.peer.address, sizeof(settings.peer.address)) != 0) {
      PRINTLN("Ignoring packet from non-paired device");
      return;
    }
  }

  if (request.generic.packetType == PACKET_TYPE_CONTROL) {
    digitalWrite(LED_BUILTIN, LOW);

    applyControl(&request.control);

    for (int channel = 0; channel < NUM_CHANNELS; channel++)
      lastChannels[channel] = request.control.channels[channel];
    hasLastChannels = true;

    for (int channel = 0; channel < NUM_CHANNELS; channel++) {
      PRINT(F("ch"));
      PRINT(channel + 1);
      PRINT(F(": "));
      PRINT(request.control.channels[channel]);
      if (channel < NUM_CHANNELS - 1) {
        PRINT(F(", "));
      } else {
        PRINTLN();
      }
    }

    if (now - sendTelemetryTime > 5000) {
      sendTelemetry(requestMac);
      sendTelemetryTime = now;
    }

    digitalWrite(LED_BUILTIN, HIGH);
  } else if (request.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
    settings.rfChannel = request.rfChannel.rfChannel;
    PRINT("New RF channel: ");
    PRINTLN(settings.rfChannel);
    esp_now_set_peer_channel(requestMac, rfChannelToWifi(settings.rfChannel));
    saveSettings();
  } else if (request.generic.packetType == PACKET_TYPE_COMMAND) {
    if (request.command.command == COMMAND_SAVE_FAILSAFE && hasLastChannels) {
      PRINT(F("Saving state for failsafe"));
      request.control.packetType = PACKET_TYPE_CONTROL;
      for (int channel = 0; channel < NUM_CHANNELS; channel++)
        request.control.channels[channel] = lastChannels[channel];
      memcpy(&settings.failsafe, &request.control, sizeof(ControlPacket));
      saveSettings();
    }
  }
}

unsigned long vHist[5] = {0, 0, 0, 0, 0};
byte vHistPos = 0;

void updateBattaryVoltage() {
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

  WiFi.mode(WIFI_STA);
  PRINT("MAC: ");
  PRINTLN(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP: init FAIL");
    return;
  }
  PRINTLN("ESP: init OK");

  EEPROM.begin(sizeof(Settings));
  loadSettings();
  if (settings.isPaired) {
    PRINTLN("Starting paired");
  } else {
    PRINTLN("Starting unpaired");
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);

  requestTime = 0;
  processTime = 0;
  sendTelemetryTime = 0;
}

void loop() {
  if (requestTime > processTime)
    processRequest();

  if (!isFailsafe && requestTime > 0 && millis() - requestTime > 1250) {
    PRINTLN(F("Radio signal lost"));
    isFailsafe = true;
    applyControl(&settings.failsafe);
  }

  if (digitalRead(PAIR_PIN) == LOW && !isPairing) {
    isPairing = true;
    settings.isPaired = false;
    PRINTLN("Paring");
  }
}

// vim:et:ai:sw=2
