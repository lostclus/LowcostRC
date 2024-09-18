#include <string.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <LowcostRC_Rx_ESP8266.h>
#include <LowcostRC_Console.h>

#define ESP_OK 0
#define DEFAULT_WIFI_CHANNEL 11

static ESP8266Receiver *receiver = NULL;

void onDataRecv(uint8_t *mac,  uint8_t *incomingData, uint8_t len) {
  if (receiver != NULL)
    receiver->_onDataRecv(mac, incomingData, len);
}

ESP8266Receiver::ESP8266Receiver()
  : address(ADDRESS_NONE),
    peer(ADDRESS_NONE),
    requestTime(0),
    receiveTime(0),
    isPairing(false),
    _isPaired(false)
{
  if (receiver == NULL) {
    receiver = this;
  } else {
    PRINTLN("ESP: Error: Only one receiver instance allowed");
  }
}

uint8_t ESP8266Receiver::rfChannelToWifi(RFChannel ch) {
  if (ch == DEFAULT_RF_CHANNEL)
    return DEFAULT_WIFI_CHANNEL;
  return ch;
}

void ESP8266Receiver::ensurePeerExist(uint8_t *mac, uint8_t wifiChannel) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, wifiChannel, NULL, 0);
  } else {
    esp_now_set_peer_channel(mac, wifiChannel);
  }
}

void ESP8266Receiver::_onDataRecv(uint8_t *mac,  uint8_t *incomingData, uint8_t len) {
  if (len < sizeof(RequestPacket)) {
    PRINT("ESP: Invalid packet size: ");
    PRINTLN(len);
    return;
  }

  if (requestTime > receiveTime) {
    PRINTLN("ESP: Ignoring extra packet");
    return;
  }

  if (!isPairing) {
    if (!_isPaired) {
      PRINTLN("ESP: Ignoring packet in unpaired state");
      return;
    } else {
      if (memcmp(mac, peer.address, ADDRESS_LENGTH) != 0) {
        PRINTLN("ESP: Ignoring packet from non-paired device");
        return;
      }
    }
  }

  memcpy(&request, incomingData, sizeof(RequestPacket));
  memcpy(requestMac, mac, sizeof(requestMac));
  requestTime = millis();
}

bool ESP8266Receiver::begin(const Address *peerAddr, RFChannel ch) {
  Address noneAddr = ADDRESS_NONE;

  WiFi.mode(WIFI_STA);

  PRINT("ESP: MAC: ");
  PRINTLN(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    PRINTLN("ESP: init: FAIL");
    return false;
  }

  PRINTLN("ESP: init: OK");

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);

  WiFi.macAddress(address.address);

  memcpy(peer.address, peerAddr->address, ADDRESS_LENGTH);
  _isPaired = memcmp(peer.address, noneAddr.address, ADDRESS_LENGTH) != 0;

  if (_isPaired) {
    PRINTLN("ESP: Starting paired");
    ensurePeerExist(peer.address, rfChannelToWifi(ch));
  } else {
    PRINTLN("ESP: Starting unpaired");
  }

  return true;
}

const Address *ESP8266Receiver::getAddress() {
  return &address;
}

const Address *ESP8266Receiver::getPeerAddress() {
  return &peer;
}

RFChannel ESP8266Receiver::getRFChannel() {
  return rfChannel;
}

void ESP8266Receiver::setRFChannel(RFChannel ch) {
  rfChannel = ch;
  esp_now_set_peer_channel(peer.address, rfChannelToWifi(rfChannel));
}

void ESP8266Receiver::setPALevel(PALevel level) {
}

bool ESP8266Receiver::receive(RequestPacket *packet) {
  if (receiveTime == requestTime)
    return false;

  memcpy(packet, &request, sizeof(RequestPacket));
  receiveTime = requestTime;
  return true;
}

void ESP8266Receiver::send(const ResponsePacket *packet) {
  if (esp_now_send(peer.address, (uint8_t*)packet, sizeof(ResponsePacket)) != ESP_OK) {
    PRINTLN("ESP: Error sending packet");
  }
}

bool ESP8266Receiver::pair() {
  RequestPacket req;
  ResponsePacket resp;
  int readyCount = 0;

  PRINTLN("ESP: Starting pairing");
  isPairing = true;

  for (
    unsigned long start = millis();
    millis() - start < 10000;
  ) {
    if (receive(&req)) {
      if (req.pair.packetType != PACKET_TYPE_PAIR) {
        PRINTLN("ESP: Ignoring non-pair type packet");
        continue;
      }

      readyCount = 0;
      ensurePeerExist(requestMac, rfChannelToWifi(DEFAULT_RF_CHANNEL));

      if (req.pair.status == PAIR_STATUS_INIT) {
        PRINT("ESP: Ready to pair in session: ");
        PRINTLN(req.pair.session);
        resp.pair.packetType = PACKET_TYPE_PAIR;
        resp.pair.status = PAIR_STATUS_READY;
        resp.pair.session = req.pair.session;
        WiFi.macAddress(resp.pair.sender.address);
        readyCount = 10;
      } else if (req.pair.status == PAIR_STATUS_PAIRED) {
        PRINTLN("ESP: Paired");
        isPairing = false;
        _isPaired = true;
        memcpy(peer.address, requestMac, ADDRESS_LENGTH);
        return true;
      }
    } else {
      delay(50);
    }
    if (readyCount) {
      readyCount--;
      PRINTLN("ESP: Sending pair ready response");
      if (esp_now_send(requestMac, (uint8_t*)&resp, sizeof(ResponsePacket)) != ESP_OK) {
        PRINTLN("ESP: Error sending pair ready response");
      }
      delay(5);
    }
  };

  PRINTLN("ESP: Not paired");
  isPairing = false;
  return false;
}

bool ESP8266Receiver::isPaired() {
  return _isPaired;
}

// vim:ai:sw=2:et
