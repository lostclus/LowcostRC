#ifndef Buzzer_h
#define Buzzer_h

#define BEEP_LOW_HZ (2 * 440)
#define BEEP_HIGH_HZ (5 * 440)

class Buzzer {
  private:
    int pin;
    unsigned long beepTime;
    bool beepState;
    unsigned int beepFreq,
                 beepDuration,
                 beepPause,
                 beepCount;

  public:
    Buzzer(int pin);
    void begin();
    void beep(
      unsigned int freq,
      unsigned int duration,
      unsigned int pause,
      unsigned int count
    );
    void noBeep();
    void handle();
};

#endif // Buzzer_h
// vim:ai:sw=2:et
