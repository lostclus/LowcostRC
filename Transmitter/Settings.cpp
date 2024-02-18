#include <Arduino.h>
#include <string.h>
#include <EEPROM.h>
#include <LowcostRC_Console.h>
#include "Settings.h"

#define SETTINGS_SIZE sizeof(SettingsValues)

const int DEFAULT_BATTARY_LOW_MV = 3400,
          DEFAULT_JOY_CENTER = 512,
          DEFAULT_JOY_THRESHOLD = 1;
const bool DEFAULT_JOY_INVERT = false;
const int DEFAULT_DUAL_RATE = 900,
          DEFAULT_TRIMMING = 0,
          DEFAULT_SWITCH_LOW = 1000,
          DEFAULT_SWITCH_HIGH = 2000;

const SettingsValues defaultSettingsValues PROGMEM = {
  SETTINGS_MAGICK,
  DEFAULT_RF_CHANNEL,
  DEFAULT_BATTARY_LOW_MV,
  // axes
  {
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL1
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL2
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL3
    },
    {
      DEFAULT_JOY_CENTER,
      DEFAULT_JOY_THRESHOLD,
      DEFAULT_JOY_INVERT,
      DEFAULT_DUAL_RATE,
      DEFAULT_TRIMMING,
      CHANNEL4
    }
  },
  // switches
  {
    {
      DEFAULT_SWITCH_LOW,
      DEFAULT_SWITCH_HIGH,
      CHANNEL5
    },
    {
      DEFAULT_SWITCH_LOW,
      DEFAULT_SWITCH_HIGH,
      CHANNEL6
    }
  }
};

bool Settings::loadProfile() {
  PRINT(F("Reading profile #"));
  PRINT(currentProfile);
  PRINTLN(F(" from flash ROM..."));
  EEPROM.get(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, values);

  if (values.magick != SETTINGS_MAGICK) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&values, &defaultSettingsValues, SETTINGS_SIZE);
    return false;
  } 

  PRINTLN(F("Using stored settings in flash ROM"));
  return true;
}

void Settings::saveProfile() {
  EEPROM.put(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, values);
}
