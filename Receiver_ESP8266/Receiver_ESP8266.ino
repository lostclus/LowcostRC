#include <string.h>
#include <ESP_EEPROM.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <Servo.h>
#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0
#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x1234
#define PAIR_PIN D3
#define CHANNEL1_PIN 4

Servo channel1Servo;

RequestPacket req;
ResponsePacket resp;
unsigned long controlTime,
              sendTelemetryTime;
bool isPairing = false;

struct Settings {
  uint16_t magick;
  bool isPaired;
  Address peer;
  RFChannel rfChannel;
} settings;

const struct Settings defaultSettings = {
  SETTINGS_MAGICK,
  false,
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL
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
  if (control->channels[CHANNEL1])
    channel1Servo.writeMicroseconds(control->channels[CHANNEL1]);
}

void ensurePeerExist(uint8_t *mac, uint8_t wifiChannel) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, wifiChannel, NULL, 0);
  } else {
    esp_now_set_peer_channel(mac, wifiChannel);
  }
}

void sendPairReadyResponse(uint8_t *mac, uint16_t session) {
  resp.pair.packetType = PACKET_TYPE_PAIR;
  resp.pair.status = PAIR_STATUS_READY;
  resp.pair.session = session;
  WiFi.macAddress(resp.pair.sender.address);
  if (esp_now_send(mac, (uint8_t*)&resp, sizeof(resp)) != ESP_OK) {
    PRINTLN("Error sending pair ready response");
  }
}

void sendTelemetry(uint8_t *mac) {
  resp.telemetry.packetType = PACKET_TYPE_TELEMETRY;
  resp.telemetry.battaryMV = 3700;
  if (esp_now_send(mac, (uint8_t*)&resp, sizeof(resp)) != ESP_OK) {
    PRINTLN("Error sending telemetry");
  }
}

void OnDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len) {
  unsigned long now = millis();

  if (len < sizeof(RequestPacket)) {
    PRINT("Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  memcpy(&req, incomingData, sizeof(RequestPacket));

  if (!settings.isPaired) {
    if (isPairing && req.pair.packetType == PACKET_TYPE_PAIR) {
      if (req.pair.status == PAIR_STATUS_INIT) {
	PRINTLN("Recived pair init packet");
	ensurePeerExist(mac, rfChannelToWifi(DEFAULT_RF_CHANNEL));
	sendPairReadyResponse(mac, req.pair.session);
	return;
      } else if (req.pair.status == PAIR_STATUS_PAIRED) {
	PRINTLN("Paired");
        isPairing = false;
	settings.isPaired = true;
	memcpy(settings.peer.address, mac, ADDRESS_LENGTH);
	ensurePeerExist(mac, rfChannelToWifi(DEFAULT_RF_CHANNEL));
	settings.rfChannel = DEFAULT_RF_CHANNEL;
	saveSettings();
	sendTelemetry(mac);
	return;
      }
    }
    PRINTLN("Ignoring packet in unpaired state");
    return;
  } else {
    if (memcmp(mac, settings.peer.address, sizeof(settings.peer.address)) != 0) {
      PRINTLN("Ignoring packet from non-paired device");
      return;
    }
  }

  if (req.generic.packetType == PACKET_TYPE_CONTROL) {
    digitalWrite(LED_BUILTIN, LOW);

    applyControl(&req.control);

    for (int channel = 0; channel < NUM_CHANNELS; channel++) {
      PRINT(F("ch"));
      PRINT(channel + 1);
      PRINT(F(": "));
      PRINT(req.control.channels[channel]);
      if (channel < NUM_CHANNELS - 1) {
	PRINT(F(", "));
      } else {
	PRINTLN();
      }
    }

    if (now - sendTelemetryTime > 5000) {
      sendTelemetry(mac);
      sendTelemetryTime = now;
    }

    digitalWrite(LED_BUILTIN, HIGH);
  } else if (req.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
    settings.rfChannel = req.rfChannel.rfChannel;
    PRINT("New RF channel: ");
    PRINTLN(settings.rfChannel);
    esp_now_set_peer_channel(mac, rfChannelToWifi(settings.rfChannel));
    saveSettings();
  }
}

void setup() {
#ifdef WITH_CONSOLE
  Serial.begin(115200);
#endif
  pinMode(PAIR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  channel1Servo.attach(CHANNEL1_PIN);

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

  controlTime = 0;
  sendTelemetryTime = 0;
}

void loop() {
  if (digitalRead(PAIR_PIN) == LOW && !isPairing) {
    isPairing = true;
    settings.isPaired = false;
    PRINTLN("Paring");
  }
}

// vim:et:ai:sw=2
