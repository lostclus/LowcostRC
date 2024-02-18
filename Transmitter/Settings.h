#ifndef Settings_h
#define Settings_h

#include <LowcostRC_Protocol.h>
#include "Types.h"

#define SETTINGS_MAGICK 0x5558
#define PROFILES_ADDR 0
#define NUM_PROFILES 5

struct AxisSettings {
  int joyCenter,
      joyThreshold;
  bool joyInvert;
  int dualRate,
      trimming;
  ChannelN channel;
};

struct SwitchesSettings {
  int low, high;
  ChannelN channel;
};

struct SettingsValues {
  int magick;
  int rfChannel;
  int battaryLowMV;
  AxisSettings axes[AXES_COUNT];
  SwitchesSettings switches[SWITCHES_COUNT];
};

class Settings {
  public:
    SettingsValues values;
    int currentProfile = 0;

    bool loadProfile();
    void saveProfile();
};

#endif // Settings_h
// vim:et:sw=2:ai
