#include <string.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <SPISlave.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_SPI.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0

const int addressBookLength = 5;
uint8_t addressBook[addressBookLength][6] = {
  // {0x80, 0x7d, 0x3A, 0x7f, 0xdd, 0xf6},
  {0x24, 0xd7, 0xeb, 0xc8, 0xd4, 0x59},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0}
};
int curAddressNum = 0;

TelemetryPacket telemetry;


bool hasAddressBookRecord(int n) {
  return (addressBook[n][0] != 0 && addressBook[n][1] != 0);
}

void onSPIStatus(uint32_t status) {
  if (status >= STATUS_SET_RF_CHANNEL && status < STATUS_SET_RF_CHANNEL + addressBookLength) {
    curAddressNum = status - STATUS_SET_RF_CHANNEL;
    PRINT("New address: #");
    PRINTLN(curAddressNum);
  }
}

void onSPIDataRecv(uint8_t *data, size_t len) {
  RequestPacket *rp = (RequestPacket*)data;

  if (len != SPI_PACKET_SIZE) {
    PRINT("SPI: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  if (!hasAddressBookRecord(curAddressNum)) {
    PRINTLN("No receiver address");
    SPISlave.setStatus(STATUS_FAILURE);
    return;
  }

  if (esp_now_send(addressBook[curAddressNum], data, sizeof(RequestPacket)) != ESP_OK) {
    PRINTLN("Error sending data to the receiver");
    SPISlave.setStatus(STATUS_FAILURE);
  }
}

void onESPNowDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == ESP_OK) {
    PRINTLN("ESP-NOW: delivery OK");
    SPISlave.setStatus(STATUS_OK);
  } else {
    PRINTLN("ESP-NOW: delivery FAIL");
    SPISlave.setStatus(STATUS_FAILURE);
  }
}

void onESPNowDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len) {
  if (len != sizeof(TelemetryPacket)) {
    PRINT("ESP-NOW: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  if (memcmp(mac, addressBook[curAddressNum], sizeof(addressBook[0])) != 0) {
    PRINTLN("ESP-NOW: Invalid sender address");
    return;
  }

  memcpy(&telemetry, incomingData, sizeof(telemetry));

  if (telemetry.packetType != PACKET_TYPE_TELEMETRY) {
    PRINT("ESP-NOW: Invalid packet type: ");
    PRINTLN(telemetry.packetType);
    return;
  }

  PRINTLN("ESP-NOW: Received telemetry");

  // SPISlave.setStatus(STATUS_TELEMETRY);
  SPISlave.setData((uint8_t*)&telemetry, sizeof(telemetry));
}

void setup() {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  #endif

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP-NOW: init FAIL");
    return;
  }
  PRINTLN("ESP-NOW: init OK");

  //esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);
  esp_now_register_send_cb(onESPNowDataSent);

  for (int i=0; i < addressBookLength; i++)
    if (hasAddressBookRecord(i))
      esp_now_add_peer(addressBook[i], ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  SPISlave.onStatus(onSPIStatus);
  SPISlave.onData(onSPIDataRecv);
  SPISlave.begin();
}

void loop() {
}

// vim:ai:sw=2
