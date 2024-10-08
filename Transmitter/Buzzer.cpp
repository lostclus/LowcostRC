#include <Arduino.h>
#include "Config.h"
#include "Buzzer.h"

Buzzer::Buzzer()
  : beepTime(0)
  , beepState(false)
  , beepFreq(0)
  , beepDuration(0)
  , beepPause(0)
  , beepCount(0)
{
}

void Buzzer::begin() {
  pinMode(BUZZER_PIN, OUTPUT);
}

void Buzzer::beep(
  unsigned int freq,
  unsigned int duration,
  unsigned int pause,
  unsigned int count
) {
  beepFreq = freq;
  beepDuration = duration;
  beepPause = pause;
  beepCount = count;
}

void Buzzer::noBeep() {
  beepFreq = 0;
  beepDuration = 0;
  beepPause = 0;
  beepCount = 0;
}

void Buzzer::handle() {
  unsigned long now = millis();

  if (beepCount > 0) {
    if (!beepState) {
      if (now - beepTime > beepPause) {
        beepState = true;
        beepTime = now;
      }
    } else {
      if (now - beepTime > beepDuration) {
        beepState = false;
        beepTime = now;
        beepCount--;
      }
    } 
  }
  if (beepState) {
    tone(BUZZER_PIN, beepFreq);
  } else {
    noTone(BUZZER_PIN);
  }
}

// vim:ai:sw=2:et
