#include <Arduino.h>
#include <LowcostRC_Console.h>
#include <LowcostRC_VoltMetter.h>

VoltMetter::VoltMetter(int pin, unsigned long r1, unsigned long r2)
  : pin(pin),
    r1(r1),
    r2(r2)
{
}

unsigned int VoltMetter::readMillivolts() {
  byte i, count = 0;
  unsigned long vcc = 0, vpin, vsum = 0;
   
#ifdef ARDUINO_ARCH_AVR
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
      ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
      ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
      ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
      // works on an Arduino 168 or 328
      ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  delay(3); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  // 1.1 * 1023 * 1000 = 1125300
  vcc = 1125300L / ((unsigned long)((high<<8) | low));
#elif defined(ARDUINO_ARCH_ESP8266)
  vcc = 1000L;
#endif
  vpin = analogRead(pin);

  /*
  PRINT(F("vcc: "));
  PRINTLN(vcc);

  PRINT(F("vpin: "));
  PRINTLN(vpin);
  */

  vHist[vHistPos] = vpin * vcc;
  vHistPos = (vHistPos + 1) % (sizeof(vHist) / sizeof(vHist[0]));

  for (i = 0; i < sizeof(vHist) / sizeof(vHist[0]); i++) {
    if (vHist[i] > 0) {
      vsum += vHist[i];
      count += 1;
    }
  }

  return (vsum / count) / 1024 * (1000L / (r2 * 1000L / (r1 + r2)));
}

// vim:ai:sw=2:et
