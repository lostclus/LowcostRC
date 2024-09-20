#ifndef LOWCOSTRC_OUTPUT_H
#define LOWCOSTRC_OUTPUT_H

#include <Servo.h>

class BaseOutput {
  public:
    virtual void begin() = 0;
    virtual void write(uint16_t value) = 0;
};

class NullOutput : public BaseOutput {
  public:
    virtual void begin();
    virtual void write(uint16_t value);
};

class PWMMicrosecondsOutput : public BaseOutput {
  private:
    int pin;
    Servo servo;
  public:
    PWMMicrosecondsOutput(int pin);
    virtual void begin();
    virtual void write(uint16_t value);
};

class PWMDutyCycleOutput : public BaseOutput {
  private:
    int pin;
    uint16_t minValue, maxValue;
  public:
    PWMDutyCycleOutput(int pin);
    PWMDutyCycleOutput(int pin, uint16_t minValue, uint16_t maxValue);
    virtual void begin();
    virtual void write(uint16_t value);
};

class BooleanOutput : public BaseOutput {
  private:
    int pin;
    uint16_t minValue, maxValue;
  public:
    BooleanOutput(int pin);
    BooleanOutput(int pin, uint16_t minValue, uint16_t maxValue);
    virtual void begin();
    virtual void write(uint16_t value);
};

#endif // LOWCOSTRC_OUTPUT_H
// vim:et:sw=2:ai
