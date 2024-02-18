#include <string.h>
#include <EEPROM.h>
#include <espnow.h>
#include <ESP8266WiFi.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0

const unsigned settingsMagick = 0x1234;

RequestPacket rp;
TelemetryPacket telemetry;
unsigned long controlTime,
              sendTelemetryTime;

struct Settings {
  unsigned magick;
  bool isPaired;
  uint8_t peer[6];
  int wifiChannel;
};

struct Settings settings;
const struct Settings defaultSettings = {
  settingsMagick,
  false,
  {0, 0, 0, 0, 0, 0},
  1
};

bool loadSettings() {
  EEPROM.get(0, settings);
  if (settings.magick != settingsMagick) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy(&settings, &defaultSettings, sizeof(settings));
    return false;
  }
  PRINTLN("Using stored settings in flash ROM");
  return true;
}

void saveSettings() {
  //EEPROM.put(0, settings);
}

void ensurePeerExist(uint8_t *mac) {
  if (!esp_now_is_peer_exist(mac))
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
}

void sendTelemetry(uint8_t *mac) {
  telemetry.packetType = PACKET_TYPE_TELEMETRY;
  telemetry.battaryMV = 3700;
  if (esp_now_send(mac, (uint8_t*)&telemetry, sizeof(telemetry)) != ESP_OK) {
    PRINTLN("Error sending telemetry");
  }
}

void OnDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len) {
  unsigned long now = millis();

  if (len < sizeof(rp)) {
    PRINT("Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  memcpy(&rp, incomingData, sizeof(rp));

  if (!settings.isPaired) {
    if (rp.generic.packetType == PACKET_TYPE_COMMAND) {
      if (rp.command.command == COMMAND_PAIR) {
	PRINTLN("Recived pair request");
	ensurePeerExist(mac);
	esp_now_set_peer_channel(mac, 1);
	sendTelemetry(mac);
	return;
      } else if (rp.command.command == COMMAND_PAIRED) {
	PRINTLN("Is paired");
	settings.isPaired = true;
	memcpy(settings.peer, mac, sizeof(settings.peer));
	ensurePeerExist(mac);
	esp_now_set_peer_channel(mac, 1);
	settings.wifiChannel = 1;
	saveSettings();
	sendTelemetry(mac);
	return;
      }
    }
    PRINTLN("Ignoring packet in unpaired state");
    return;
  } else {
    if (memcmp(mac, settings.peer, sizeof(settings.peer)) != 0) {
      PRINTLN("Ignoring packet from non-paired device");
      return;
    }
  }

  if (rp.generic.packetType == PACKET_TYPE_CONTROL) {
    digitalWrite(LED_BUILTIN, LOW);

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

    if (now - sendTelemetryTime > 5000) {
      sendTelemetry(mac);
      sendTelemetryTime = now;
    }

    digitalWrite(LED_BUILTIN, HIGH);
  } else if (rp.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
    settings.wifiChannel = (rp.rfChannel.rfChannel % 10) + 1;
    PRINT("New wifi channel: ");
    PRINTLN(settings.wifiChannel);
    esp_now_set_peer_channel(mac, settings.wifiChannel);
    saveSettings();
  }
}

void setup() {
#ifdef WITH_CONSOLE
  Serial.begin(115200);
#endif
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.mode(WIFI_STA);
  PRINT("MAC: ");
  PRINTLN(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP-NOW: init FAIL");
    return;
  }
  PRINTLN("ESP-NOW: init OK");

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
}

// vim:ai:sw=2
