#include <RF24.h>
#include <Servo.h>

#include "protocol.h"

#undef WITH_CONSOLE
//define WITH_CONSOLE

#define MOTOR_PWM_PIN 3
#define ALERONS_SERVO_PIN 4
#define ELEVATOR_SERVO_PIN 5

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define VOLT_METER_PIN A0
#define VOLT_METER_R1 10000L
#define VOLT_METER_R2 10000L

unsigned int battaryMV = 0;
unsigned long requestTime,
              battaryUpdateTime;

Servo aleronsServo, elevatorServo;
RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

void setup(void) {
  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_PWM_PIN, LOW);

  analogReference(DEFAULT);

  aleronsServo.attach(ALERONS_SERVO_PIN);
  elevatorServo.attach(ELEVATOR_SERVO_PIN);

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN(F("Radio init: FAILURE"));
  }
  radio.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  radio.setPayloadSize(16);
  radio.openReadingPipe(1, RADIO_PIPE);
  radio.enableAckPayload();
  radio.startListening();

  request.alerons = 0;
  request.elevator = 0;
  request.engine = 0;

  requestTime = 0;
  battaryUpdateTime = 0;

  response.magick = MAGICK;
  response.battaryMV = 0;

  aleronsServo.writeMicroseconds(mapAlerons(0));
  elevatorServo.writeMicroseconds(mapElevator(0));
  analogWrite(MOTOR_PWM_PIN, 0);
}

void loop(void) {
  unsigned long now = millis();
  int alerons, elevator, engine;

  if (now - battaryUpdateTime > 5000) {
    battaryUpdateTime = now;
    updateBattaryVoltage();
    PRINT(F("battaryMV: "));
    PRINTLN(battaryMV);
    response.battaryMV = battaryMV;
    radio.writeAckPayload(1, &response, sizeof(response));
  }

  if (radio.available()) {
    radio.read(&request, sizeof(request));
    if (request.magick == MAGICK) {
      requestTime = now;

      alerons = mapAlerons(request.alerons);
      elevator = mapElevator(request.elevator);
      engine = request.engine;

      #ifdef WITH_CONSOLE
      if (now / 1000 % 2 == 0) {
        PRINT(F("alerons: "));
        PRINT(alerons);
        PRINT(F(", "));
        PRINT(F("elevator: "));
        PRINT(elevator);
        PRINT(F(", "));
        PRINT(F("engine: "));
        PRINTLN(engine);
      }
      #endif

      // aleronsServo.write(alerons);
      // elevatorServo.write(elevator);
      aleronsServo.writeMicroseconds(alerons);
      elevatorServo.writeMicroseconds(elevator);
      analogWrite(MOTOR_PWM_PIN, engine);
    }
  }

  if (requestTime > 0 && now - requestTime > 1250) {
    PRINTLN(F("Radio signal lost"));
    aleronsServo.writeMicroseconds(mapAlerons(0));
    elevatorServo.writeMicroseconds(mapElevator(0));
    analogWrite(MOTOR_PWM_PIN, 0);
  }
}

int mapAlerons(int value) {
  return map(value, -16000, 16000, 700, 2300);
}

int mapElevator(int value) {
  return map(value, -16000, 16000, 700, 2300);
}

unsigned long vHist[5] = {0, 0, 0, 0, 0};
byte vHistPos = 0;

void updateBattaryVoltage() {
  byte i, count = 0;
  unsigned long vcc = 0, vpin, vsum = 0;
   
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
  vpin = analogRead(VOLT_METER_PIN);

  PRINT(F("vcc: "));
  PRINTLN(vcc);

  PRINT(F("vpin: "));
  PRINTLN(vpin);

  vHist[vHistPos] = vpin * vcc;
  vHistPos = (vHistPos + 1) % (sizeof(vHist) / sizeof(vHist[0]));

  for (i = 0; i < sizeof(vHist) / sizeof(vHist[0]); i++) {
    if (vHist[i] > 0) {
      vsum += vHist[i];
      count += 1;
    }
  }

  battaryMV = (vsum / count) / 1024 
    * (1000L / (VOLT_METER_R2 * 1000L / (VOLT_METER_R1 + VOLT_METER_R2)));
}

// vim:ai:sw=2:et
