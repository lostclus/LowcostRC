#include <string.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <SPISlave.h>

#include <LowcostRC_Protocol.h>
#include <LowcostRC_SPI.h>
#include <LowcostRC_Tx.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0
#define RANDOM_SEED_PIN A0
#define ESP8266_DEFAULT_CHANNEL 11
#define ESP8266_NUM_CHANNELS 12

Address peer = ADDRESS_NONE;
RFChannel rfChannel = DEFAULT_RF_CHANNEL;

uint16_t pairSession = 0;

bool blinkState = false;
unsigned long blinkTime = 0;
int blinkDuration = 0,
    blinkPause = 0,
    blinkCount = 0;

uint8_t rfChannelToWifi(RFChannel ch) {
  if (ch == DEFAULT_RF_CHANNEL)
    return ESP8266_DEFAULT_CHANNEL;
  return ch;
}

void onSPIStatus(uint32_t status) {
  SPIResponsePacket resp;

  if (status == SPI_STATUS_STARTING) {
    PRINTLN("Starting");
    resp.init.packetType = SPI_PACKET_TYPE_INIT;
    resp.init.moduleType = MODULE_TYPE_ESP8266;
    resp.init.numRFChannels = ESP8266_NUM_CHANNELS;
    resp.init.numPALevels = 1;
    SPISlave.setData((uint8_t*)&resp, sizeof(ResponsePacket));
    SPISlave.setStatus(SPI_STATUS_OK);
  }
  else if (status == SPI_STATUS_PAIRING) {
    PRINTLN("Pairing");
    blinkCount = -1;
    blinkDuration = 50;
    blinkPause = 50;
    pairSession = random(1, 1 << 15);
    sendPairInitPacket(pairSession);
  }
}

void onSPIDataRecv(uint8_t *data, size_t len) {
  SPIRequestPacket req;

  if (len != SPI_PACKET_SIZE) {
    PRINT("SPI: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  if (pairSession) return;

  memcpy(&req, data, sizeof(SPIRequestPacket));

  if (req.peerAddr.packetType == SPI_PACKET_TYPE_SET_PEER_ADDRESS) {
    PRINTLN("SPI: Setting new peer address");
    memcpy(peer.address, req.peerAddr.peer.address, ADDRESS_LENGTH);
    if (!esp_now_is_peer_exist(peer.address)) {
      esp_now_add_peer(
        peer.address, ESP_NOW_ROLE_COMBO, rfChannelToWifi(rfChannel), NULL, 0
      );
    }
    SPISlave.setStatus(SPI_STATUS_OK);
  }
  else if (req.rfChannel.packetType == SPI_PACKET_TYPE_SET_RF_CHANNEL) {
    PRINT("SPI: Setting new RF channel: ");
    PRINTLN(req.rfChannel.rfChannel);
    rfChannel = req.rfChannel.rfChannel;
    esp_now_set_peer_channel(peer.address, rfChannelToWifi(rfChannel));
    SPISlave.setStatus(SPI_STATUS_OK);
  } else if (req.rfChannel.packetType == SPI_PACKET_TYPE_SET_PA_LEVEL) {
    // TODO: do research if PA level can be adjested on ESP8266 for ESP-NOW
    // packets
    SPISlave.setStatus(SPI_STATUS_OK);
  } else {
    if (
      esp_now_send(
	peer.address, (uint8_t*)&req.request, sizeof(RequestPacket)
      ) != ESP_OK
    ) {
      PRINTLN("Error sending data to the receiver");
      SPISlave.setStatus(SPI_STATUS_FAILURE);
    }
  }
}

void onESPNowDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == ESP_OK) {
    PRINTLN("ESP: delivery OK");
    SPISlave.setStatus(SPI_STATUS_OK);
    blinkCount = 1;
    blinkDuration = 50;
    blinkPause = 50;
  } else {
    PRINTLN("ESP: delivery FAIL");
    SPISlave.setStatus(SPI_STATUS_FAILURE);
  }
}

void onESPNowDataRecv(uint8_t * mac,  uint8_t *incomingData, uint8_t len) {
  ResponsePacket resp;

  if (len != sizeof(ResponsePacket)) {
    PRINT("ESP: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  memcpy(&resp, incomingData, sizeof(ResponsePacket));

  if (
    pairSession
    && resp.pair.packetType == PACKET_TYPE_PAIR
    && resp.pair.status == PAIR_STATUS_READY
    && resp.pair.session == pairSession
  ) {
    PRINTLN("ESP: Received pair ready response");
    blinkCount = 0;

    if (esp_now_is_peer_exist(peer.address))
      esp_now_del_peer(peer.address);

    memcpy(peer.address, mac, ADDRESS_LENGTH);
    rfChannel = DEFAULT_RF_CHANNEL;

    if (!esp_now_is_peer_exist(peer.address)) {
      esp_now_add_peer(
	peer.address, ESP_NOW_ROLE_COMBO, rfChannelToWifi(rfChannel), NULL, 0
      );
    } else {
      esp_now_set_peer_channel(peer.address, rfChannelToWifi(rfChannel));
    }

    if (sendPaired(pairSession)) {;
      PRINTLN("ESP: Paired");
      blinkCount = 0;
      pairSession = 0;
      SPISlave.setData((uint8_t*)&resp, sizeof(ResponsePacket));
      SPISlave.setStatus(SPI_STATUS_PAIRED);
      delay(100);
    }
    return;
  }

  if (memcmp(mac, peer.address, sizeof(peer.address)) != 0) {
    PRINTLN("ESP: Invalid sender address");
    return;
  }

  SPISlave.setData((uint8_t*)&resp, sizeof(ResponsePacket));
  SPISlave.setStatus(SPI_STATUS_RECEIVING);
}

bool sendPairInitPacket(uint16_t session) {
  RequestPacket req;
  Address broadcast = ADDRESS_BROADCAST;

  req.pair.packetType = PACKET_TYPE_PAIR;
  req.pair.status = PAIR_STATUS_INIT;
  req.pair.session = session;
  WiFi.macAddress(req.pair.sender.address);
  if (
    esp_now_send(
      broadcast.address, (uint8_t*)&req, sizeof(RequestPacket)
    ) != ESP_OK
  ) {
    PRINTLN("Error sending pair init packet");
    return false;
  }
  return true;
}

bool sendPaired(uint16_t session) {
  RequestPacket req;

  req.pair.packetType = PACKET_TYPE_PAIR;
  req.pair.status = PAIR_STATUS_PAIRED;
  req.pair.session = session;
  WiFi.macAddress(req.pair.sender.address);

  if (esp_now_send(peer.address, (uint8_t*)&req, sizeof(req)) != ESP_OK) {
    PRINTLN("Error sending paired notification");
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

void setup() {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);
  randomSeed(analogRead(RANDOM_SEED_PIN));

  WiFi.mode(WIFI_STA);
  PRINT("MAC: ");
  PRINTLN(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP: init FAIL");
    return;
  }
  PRINTLN("ESP: init OK");

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);
  esp_now_register_send_cb(onESPNowDataSent);

  SPISlave.onStatus(onSPIStatus);
  SPISlave.onData(onSPIDataRecv);
  SPISlave.begin();
}

void loop() {
  unsigned long now = millis();
  controlBlink(now);
}

// vim:ai:sw=2
