#include <string.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <SPISlave.h>
#include <AbleButtons.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_SPI.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0

const int pairPin = D3;
const int peersCount = 12;
const size_t addressSize = 6;
const int channelsCount = 10;

uint8_t broadcastAddress[addressSize] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t peerAddress[peersCount][addressSize] = {
  // {0x80, 0x7d, 0x3A, 0x7f, 0xdd, 0xf6},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  //{0x24, 0xd7, 0xeb, 0xc8, 0xd4, 0x59},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0}
};
int currentPeer = 0,
    wifiChannel = 1;
bool isPairing = false;
TelemetryPacket telemetry;

using Button = AblePullupClickerButton;
Button pairButton(pairPin);

bool blinkState = false;
unsigned long blinkTime = 0;
int blinkDuration = 0,
    blinkPause = 0,
    blinkCount = 0;

bool hasAddress(int peer) {
  for (int i = 0; i < addressSize; i++)
    if (peerAddress[peer][i] != 0) return true;
  return false;
}

void onSPIStatus(uint32_t status) {
  if (status >= STATUS_SET_RF_CHANNEL && status < STATUS_SET_RF_CHANNEL + peersCount * channelsCount) {
    int rfChannel = status - STATUS_SET_RF_CHANNEL;
    currentPeer = rfChannel / 10;
    wifiChannel = (rfChannel % 10) + 1;
    esp_now_set_peer_channel(peerAddress[currentPeer], wifiChannel);
    PRINT("New peer: #");
    PRINTLN(currentPeer);
    PRINT("New wifi channel: #");
    PRINTLN(wifiChannel);
  }
}

void onSPIDataRecv(uint8_t *data, size_t len) {
  if (len != SPI_PACKET_SIZE) {
    PRINT("SPI: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  if (isPairing) return;

  if (!hasAddress(currentPeer)) {
    PRINTLN("No receiver address");
    SPISlave.setStatus(STATUS_FAILURE);
    return;
  }

  if (esp_now_send(peerAddress[currentPeer], data, sizeof(RequestPacket)) != ESP_OK) {
    PRINTLN("Error sending data to the receiver");
    SPISlave.setStatus(STATUS_FAILURE);
  }
}

void onESPNowDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == ESP_OK) {
    PRINTLN("ESP-NOW: delivery OK");
    SPISlave.setStatus(STATUS_OK);
    blinkCount = 1;
    blinkDuration = 100;
    blinkPause = 100;
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

  if (isPairing) {
    PRINTLN("ESP-NOW: Received pairing response");
    isPairing = false;
    blinkCount = 0;
    if (hasAddress(currentPeer) && esp_now_is_peer_exist(peerAddress[currentPeer])) {
      esp_now_del_peer(peerAddress[currentPeer]);
    }
    memcpy(peerAddress[currentPeer], mac, addressSize);
    if (!esp_now_is_peer_exist(peerAddress[currentPeer])) {
      esp_now_add_peer(peerAddress[currentPeer], ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    } else {
      esp_now_set_peer_channel(peerAddress[currentPeer], 1);
    }
    PRINT("ESP-NOW: Paired peer: #");
    PRINTLN(currentPeer);
    sendPairedNotification();
    sendRFChannel();
    esp_now_set_peer_channel(peerAddress[currentPeer], wifiChannel);
    return;
  }

  if (memcmp(mac, peerAddress[currentPeer], sizeof(peerAddress[0])) != 0) {
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

bool sendPairRequest() {
  RequestPacket rp;

  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = COMMAND_PAIR;
  if (esp_now_send(broadcastAddress, (uint8_t*)&rp, sizeof(rp)) != ESP_OK) {
    PRINTLN("Error sending pair request");
    return false;
  }
  return true;
}

bool sendPairedNotification() {
  RequestPacket rp;

  rp.command.packetType = PACKET_TYPE_COMMAND;
  rp.command.command = COMMAND_PAIRED;
  if (esp_now_send(peerAddress[currentPeer], (uint8_t*)&rp, sizeof(rp)) != ESP_OK) {
    PRINTLN("Error sending paired notification");
    return false;
  }
  return true;
}

bool sendRFChannel() {
  RequestPacket rp;

  rp.rfChannel.packetType = PACKET_TYPE_SET_RF_CHANNEL;
  rp.rfChannel.rfChannel = currentPeer * 10 + (wifiChannel - 1);
  if (esp_now_send(peerAddress[currentPeer], (uint8_t*)&rp, sizeof(rp)) != ESP_OK) {
    PRINTLN("Error sending set rf channel packet");
    return false;
  }
  return true;
}

void controlBlink(unsigned long now) {
  if (blinkCount > 0 || blinkCount == -1) {
    if (!blinkState) {
      if (now - blinkTime > blinkPause) {
        blinkState = true;
        blinkTime = now;
      }
    } else {
      if (now - blinkTime > blinkDuration) {
        blinkState = false;
        blinkTime = now;
	if (blinkCount > 0) blinkCount--;
      }
    }
  }
  if (blinkState) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void controlPairing(unsigned long now) {
  RequestPacket rp;
  static unsigned long pairReqestTime = 0;

  if (pairButton.resetClicked() && !isPairing) {
    PRINTLN("Start pairing");
    isPairing = true;
    blinkDuration = 500;
    blinkPause = 500;
    blinkCount = -1;
  }

  if (isPairing && pairReqestTime < now - 500) {
    pairReqestTime = now;
    rp.command.packetType = PACKET_TYPE_COMMAND;
    rp.command.command = COMMAND_PAIR;
    if (esp_now_send(broadcastAddress, (uint8_t*)&rp, sizeof(RequestPacket)) != ESP_OK) {
      PRINTLN("Error sending pair request");
    }
  }
}

void setup() {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);
  pairButton.begin();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP-NOW: init FAIL");
    return;
  }
  PRINTLN("ESP-NOW: init OK");

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);
  esp_now_register_send_cb(onESPNowDataSent);

  for (int peer=0; peer < peersCount; peer++)
    if (hasAddress(peer))
      esp_now_add_peer(peerAddress[peer], ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  SPISlave.onStatus(onSPIStatus);
  SPISlave.onData(onSPIDataRecv);
  SPISlave.begin();
}

void loop() {
  unsigned long now = millis();
  pairButton.handle();
  controlBlink(now);
  controlPairing(now);
}

// vim:ai:sw=2
