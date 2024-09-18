#include <Arduino.h>
#include <string.h>
#include <EEPROM.h>
#include <LowcostRC_Console.h>
#include "Settings.h"

#define SETTINGS_MAGICK 0x555b
#define PROFILES_ADDR 0
#define SETTINGS_SIZE sizeof(SettingsValues)

#define DEFAULT_BATTARY_LOW_MV  3400
#define DEFAULT_JOY_CENTER      512
#define DEFAULT_JOY_THRESHOLD   1
#define DEFAULT_JOY_INVERT      false
#define DEFAULT_DUAL_RATE       500
#define DEFAULT_TRIMMING        0
#define DEFAULT_SWITCH_LOW      1000
#define DEFAULT_SWITCH_HIGH     2000

const SettingsValues defaultSettingsValues PROGMEM = {
  SETTINGS_MAGICK,
  {0, 0, 0, 0, 0, 0, 0, 0},
  ADDRESS_NONE,
  DEFAULT_RF_CHANNEL,
  DEFAULT_PA_LEVEL,
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
    },
    {
      DEFAULT_SWITCH_LOW,
      DEFAULT_SWITCH_HIGH,
      CHANNEL7
    },
    {
      DEFAULT_SWITCH_LOW,
      DEFAULT_SWITCH_HIGH,
      CHANNEL8
    }
  }
};

bool Settings::begin() {
  loadProfile();
  return true;
}

bool Settings::loadProfile() {
  PRINT(F("Reading profile #"));
  PRINT(currentProfile + 1);
  PRINTLN(F(" from flash ROM..."));
  EEPROM.get(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, values);

  if (values.magick != SETTINGS_MAGICK) {
    PRINTLN(F("No stored settings found, use defaults"));
    memcpy_P(&values, &defaultSettingsValues, SETTINGS_SIZE);
    isLoaded = false;
    return false;
  } 

  PRINTLN(F("Using stored settings in flash ROM"));
  isLoaded = true;
  return true;
}

void Settings::saveProfile() {
  EEPROM.put(PROFILES_ADDR + currentProfile * SETTINGS_SIZE, values);
}

// vim:ai:sw=2:et
