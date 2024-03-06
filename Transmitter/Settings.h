#ifndef Settings_h
#define Settings_h

#include <LowcostRC_Protocol.h>
#include "Types.h"

#define NUM_PROFILES 8

struct AxisSettings {
  uint16_t joyCenter,
           joyThreshold;
  bool joyInvert;
  uint16_t dualRate;
  int16_t trimming;
  ChannelN channel;
};

struct SwitchesSettings {
  uint16_t low, high;
  ChannelN channel;
};

struct SettingsValues {
  uint16_t magick;
  char profileName[8];
  Address peer;
  RFChannel rfChannel;
  uint16_t battaryLowMV;
  AxisSettings axes[AXES_COUNT];
  SwitchesSettings switches[SWITCHES_COUNT];
};

class Settings {
  public:
    SettingsValues values;
    uint8_t currentProfile = 0;
    bool isLoaded = false;

    bool begin();
    bool loadProfile();
    void saveProfile();
};

#endif // Settings_h
// vim:et:sw=2:ai
