#include <LowcostRC_Protocol.h>
#include <LowcostRC_Console.h>

#include "Buzzer.h"
#include "Radio_Control.h"
#include "Settings.h"
#include "Control_Pannel.h"
#include "Controls.h"

#define BUZZER_PIN 3

Buzzer buzzer(BUZZER_PIN);
RadioControl radioControl(&buzzer);
Settings settings;
Controls controls(&settings, &buzzer, &radioControl);
ControlPannel controlPannel(&settings, &buzzer, &radioControl, &controls);

void setup(void)
{
  bool needsSetJoystickCenter = false;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif
  PRINTLN(F("Starting..."));


  buzzer.begin();
  radioControl.begin();
  controlPannel.begin();
  controls.begin();

  if (!settings.loadProfile())
    needsSetJoystickCenter = true;

  radioControl.radio->setRFChannel(settings.values.rfChannel);

  if (needsSetJoystickCenter) {
    controls.setJoystickCenter();
  }
}

void loop(void) {
  controls.handle();
  controlPannel.handle();
  radioControl.handle();
  buzzer.handle();

  /*
  if (errorTime > 0 && now - errorTime > 250) {
    statusRadioFailure = false;
    errorTime = 0;
  }
  if (requestSendTime > 0 && now - requestSendTime > 250) {
    statusRadioSuccess = false;
  }
  */
}


// vim:ai:sw=2:et
