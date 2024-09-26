#ifndef LOWCOSTRC_RX_SETTINGS_H
#define LOWCOSTRC_RX_SETTINGS_H

#include <LowcostRC_Protocol.h>

#define SETTINGS_ADDR 0
#define SETTINGS_MAGICK 0x1235

struct SettingsValues {
  uint16_t magick;
  Address address;
  RFChannel rfChannel;
  PALevel paLevel;
  ControlPacket failsafe;
} __attribute__((__packed__));


class BaseRxSettings {
  public:
    SettingsValues values;

    virtual bool begin() = 0;
    virtual bool load() = 0;
    virtual void save() = 0;
    void setDefaults();
};

class DumbRxSettings : public BaseRxSettings {
    virtual bool begin();
    virtual bool load();
    virtual void save();
};

class EEPROMRxSettings : public BaseRxSettings {
    virtual bool begin();
    virtual bool load();
    virtual void save();
};

#endif // LOWCOSTRC_RX_SETTINGS_H
// vim:et:sw=2:ai
