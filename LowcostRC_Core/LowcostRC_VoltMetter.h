#ifndef LOWCOSTRC_VOLTMETTER_H
#define LOWCOSTRC_VOLTMETTER_H

class VoltMetter {
  private:
    int pin;
    unsigned long r1, r2;
    unsigned long vHist[5] = {0, 0, 0, 0, 0};
    byte vHistPos = 0;

  public:
    VoltMetter(int pin, unsigned long r1, unsigned long r2);
    unsigned int readMillivolts();
};

#endif // LOWCOSTRC_VOLTMETTER_H
// vim:ai:sw=2:et
