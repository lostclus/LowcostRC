#include <string.h>
#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP_EEPROM.h>
#else
#include <EEPROM.h>
#endif

#include <LowcostRC_Console.h>
#include <LowcostRC_Rx_Settings.h>

static const SettingsValues defaults PROGMEM = {
  SETTINGS_MAGICK,
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL,
  DEFAULT_PA_LEVEL,
  {
    PACKET_TYPE_CONTROL,
    {0, 0, 0, 0, 0, 0, 0, 0}
  }
};

void BaseRxSettings::setDefaults() {
  PRINTLN(F("Use default settings"));
  memcpy_P(&values, &defaults, sizeof(SettingsValues));
}

bool EEPROMRxSettings::begin() {
#ifdef ARDUINO_ARCH_ESP8266
  EEPROM.begin(sizeof(SettingsValues));
#endif
  return true;
}

bool EEPROMRxSettings::load() {
  PRINTLN(F("Reading settings from flash ROM..."));
#ifdef ARDUINO_ARCH_ESP8266
  if (EEPROM.percentUsed() < 0)
    return false;
#endif
  EEPROM.get(SETTINGS_ADDR, values);
  return values.magick == SETTINGS_MAGICK;
}

void EEPROMRxSettings::save() {
  PRINTLN(F("Writing settings to flash ROM..."));
  EEPROM.put(SETTINGS_ADDR, values);
#ifdef ARDUINO_ARCH_ESP8266
  EEPROM.commit();
#endif
}

// vim:et:sw=2:ai
