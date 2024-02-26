#include "Radio.h"

BaseRadioModule::BaseRadioModule()
  : peer(ADDRESS_NONE)
  , rfChannel(DEFAULT_RF_CHANNEL)
{
}

bool BaseRadioModule::isPaired() {
  for (int i = 0; i < ADDRESS_LENGTH; i++) {
    if (peer.address[i] != 0) return true;
  }
  return false;
}

void BaseRadioModule::unpair() {
  for (int i = 0; i < ADDRESS_LENGTH; i++) {
    peer.address[i] = 0;
  }
}

// vim:et:sw=2:ai
