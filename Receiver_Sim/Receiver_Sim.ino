#include <EEPROM.h>
#include <RF24.h>
#include <Joystick.h>

#include <LowcostRC_Protocol.h>

#undef WITH_CONSOLE
//define WITH_CONSOLE

#define RESET_RF_CHANNEL_PIN 2

#define RADIO_CE_PIN 9
#define RADIO_CSN_PIN 10

#define RF_CHANNEL_ROM_ADDR 0
#define NOLINK_ROM_ADDR 2

const ControlPacket DEFAULT_NOLINK_CONTROL = {
  PACKET_TYPE_CONTROL,
  {1500, 1500, 1500, 1500}
};

unsigned long controlTime,
              sendStatusTime;

RF24 radio(RADIO_CE_PIN, RADIO_CSN_PIN);
byte pipe[7];

Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID, 
  JOYSTICK_TYPE_MULTI_AXIS,
  0, 0,
  true, true, false,
  false, false, false,
  true, true, false, false, false
);

#ifdef WITH_CONSOLE
#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(x) __asm__ __volatile__ ("nop\n\t")
#define PRINTLN(x) __asm__ __volatile__ ("nop\n\t")
#endif

void setup(void) {
  int rfChannel = DEFAULT_RF_CHANNEL;

  #ifdef WITH_CONSOLE
  Serial.begin(115200);
  #endif

  pinMode(RESET_RF_CHANNEL_PIN, INPUT_PULLUP);

  Joystick.begin(false);
  Joystick.setRudderRange(1000, 2000);
  Joystick.setThrottleRange(1000, 2000);
  Joystick.setXAxisRange(1000, 2000);
  Joystick.setYAxisRange(1000, 2000);

  //delay(3000);

  if (radio.begin()) {
    PRINTLN(F("Radio init: OK"));
  } else {
    PRINTLN(F("Radio init: FAILURE"));
  }
  radio.setRadiation(RF24_PA_MAX, RF24_250KBPS);
  radio.setPayloadSize(PACKET_SIZE);

  if (digitalRead(RESET_RF_CHANNEL_PIN) != LOW) {
    EEPROM.get(RF_CHANNEL_ROM_ADDR, rfChannel);
    if (rfChannel > 125 || rfChannel < 0) {
      rfChannel = DEFAULT_RF_CHANNEL;
    }
  }
  radio.setChannel(rfChannel);
  PRINT(F("RF channel: "));
  PRINTLN(rfChannel);

  sprintf_P(pipe, PSTR(PIPE_FORMAT), rfChannel);
  radio.openReadingPipe(1, pipe);

  radio.enableAckPayload();
  radio.startListening();

  controlTime = 0;
  sendStatusTime = 0;
}

void loop(void) {
  unsigned long now = millis();
  union RequestPacket rp;
  static int lastChannels[NUM_CHANNELS];
  static bool hasLastChannels = false;


  if (now - sendStatusTime > 5000) {
    sendStatus();
    sendStatusTime = now;
  }

  if (radio.available()) {
    radio.read(&rp, sizeof(rp));
    if (rp.generic.packetType == PACKET_TYPE_CONTROL) {
      controlTime = now;

      for (int channel = 0; channel < NUM_CHANNELS; channel++)
        lastChannels[channel] = rp.control.channels[channel];
      hasLastChannels = true;

      #ifdef WITH_CONSOLE
      if (now / 1000 % 2 == 0) {
        PRINT(F("ch1: "));
        PRINT(rp.control.channels[CHANNEL1]);
        PRINT(F(", "));
        PRINT(F("ch2: "));
        PRINT(rp.control.channels[CHANNEL2]);
        PRINT(F(", "));
        PRINT(F("ch3: "));
        PRINT(rp.control.channels[CHANNEL3]);
        PRINT(F(", "));
        PRINT(F("ch4: "));
        PRINTLN(rp.control.channels[CHANNEL4]);
      }
      #endif

      applyControl(&rp.control);
    } else if (rp.generic.packetType == PACKET_TYPE_SET_RF_CHANNEL) {
      radio.setChannel(rp.rfChannel.rfChannel);
      sprintf_P(pipe, PSTR(PIPE_FORMAT), rp.rfChannel.rfChannel);
      radio.openReadingPipe(1, pipe);
      PRINT(F("New RF channel: "));
      PRINTLN(rp.rfChannel.rfChannel);
      EEPROM.put(RF_CHANNEL_ROM_ADDR, rp.rfChannel.rfChannel);
    } else if (rp.generic.packetType == PACKET_TYPE_COMMAND) {
      if (rp.command.command == COMMAND_SAVE_FOR_NOLINK && hasLastChannels) {
        PRINT(F("Saving state for no link"));
        rp.control.packetType = PACKET_TYPE_CONTROL;
        for (int channel = 0; channel < NUM_CHANNELS; channel++)
          rp.control.channels[channel] = lastChannels[channel];
        EEPROM.put(NOLINK_ROM_ADDR, rp.control);
      }
    }
  }

  if (controlTime > 0 && now - controlTime > 1250) {
    PRINTLN(F("Radio signal lost"));
    EEPROM.get(NOLINK_ROM_ADDR, rp.control);

    if (rp.control.packetType == PACKET_TYPE_CONTROL) {
      applyControl(&rp.control);
    } else {
      applyControl(&DEFAULT_NOLINK_CONTROL);
    }
  }
}

void applyControl(ControlPacket *control) {
  Joystick.setRudder(control->channels[CHANNEL1]);
  Joystick.setThrottle(control->channels[CHANNEL2]);
  Joystick.setXAxis(control->channels[CHANNEL3]);
  Joystick.setYAxis(control->channels[CHANNEL4]);
  Joystick.sendState();
}

void sendStatus() {
  struct StatusPacket status;

  status.packetType = PACKET_TYPE_STATUS;
  status.battaryMV = 5000;
  radio.writeAckPayload(1, &status, sizeof(status));
}

// vim:ai:sw=2:et