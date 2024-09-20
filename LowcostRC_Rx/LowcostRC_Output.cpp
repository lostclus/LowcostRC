#include <Arduino.h>
#include <LowcostRC_Output.h>

void NullOutput::begin() {
}

void NullOutput::write(uint16_t value) {
}

PWMMicrosecondsOutput::PWMMicrosecondsOutput(int pin)
  : pin(pin)
{
}

void PWMMicrosecondsOutput::begin() {
  servo.attach(pin);
}

void PWMMicrosecondsOutput::write(uint16_t value) {
  servo.writeMicroseconds(value);
}

PWMDutyCycleOutput::PWMDutyCycleOutput(int pin)
  : pin(pin)
{
  minValue = 1000;
  maxValue = 2000;
}

PWMDutyCycleOutput::PWMDutyCycleOutput(int pin, uint16_t minValue, uint16_t maxValue)
  : pin(pin),
    minValue(minValue),
    maxValue(maxValue)
{
}

void PWMDutyCycleOutput::begin() {
  pinMode(pin, OUTPUT);
  analogWrite(pin, 0);
}


void PWMDutyCycleOutput::write(uint16_t value) {
  value = constrain(value, minValue, maxValue);
  analogWrite(pin, map(value, minValue, maxValue, 0, 255));
}

BooleanOutput::BooleanOutput(int pin)
  : pin(pin)
{
  minValue = 1700;
  maxValue = 2200;
}

BooleanOutput::BooleanOutput(int pin, uint16_t minValue, uint16_t maxValue)
  : pin(pin),
    minValue(minValue),
    maxValue(maxValue)
{
}

void BooleanOutput::begin() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}


void BooleanOutput::write(uint16_t value) {
  digitalWrite(pin, (value >= minValue && value <= maxValue) ? HIGH : LOW);
}

// vim:et:sw=2:ai
