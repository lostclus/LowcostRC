#include <string.h>
#include <espnow.h>
#include <ESP8266WiFi.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0

RequestPacket rp;
TelemetryPacket telemetry;
unsigned long controlTime,
              sendTelemetryTime;

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

  //esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);

  controlTime = 0;
  sendTelemetryTime = 0;
}

void loop() {
}

// vim:ai:sw=2
