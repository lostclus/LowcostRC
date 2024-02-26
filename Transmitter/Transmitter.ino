#include <LowcostRC_Console.h>

#include "Buzzer.h"
#include "Radio_Control.h"
#include "Settings.h"
#include "Control_Pannel.h"
#include "Controls.h"
#include "Config.h"

Buzzer buzzer;
RadioControl radioControl(&buzzer);
Settings settings;
Controls controls(&settings, &buzzer, &radioControl);
ControlPannel controlPannel(&settings, &buzzer, &radioControl, &controls);

void setup(void)
{
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));

  randomSeed(analogRead(RANDOM_SEED_PIN));

  buzzer.begin();
  radioControl.begin();
  controls.begin();
  settings.begin();
  controlPannel.begin();

  if (!settings.isLoaded) {
    controls.setJoystickCenter();
  }
}

void loop(void) {
  controls.handle();
  radioControl.handle();
  controlPannel.handle();
  buzzer.handle();
}

// vim:ai:sw=2:et
