#ifndef Config_h
#define Config_h

#define BUZZER_PIN 3

#define JOYSTICK_PINS {A1, A0, A3, A2}
#define SWITCH_PINS {4, 5}

#define VOLT_METER_PIN A6
#define VOLT_METER_R1 10000L
#define VOLT_METER_R2 10000L

#define RADIO_NRF24_CE_PIN 9
#define RADIO_NRF24_CSN_PIN 10

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_ADDRESS 0x3C

#if !defined(WITH_ADAFRUIT_SSD1306) && !defined(WITH_SSD1306_ASCII)
#define WITH_ADAFRUIT_SSD1306
#endif

#define SETTINGS_PIN 6
#define SETTINGS_PLUS_PIN 7
#define SETTINGS_MINUS_PIN 8

#endif // Config_h
