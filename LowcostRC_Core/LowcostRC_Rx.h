#ifndef LOWCOSTRC_RX_H
#define LOWCOSTRC_RX_H

#include <LowcostRC_Protocol.h>

class BaseReceiver {
  public:
    virtual const Address *getAddress() = 0;     // receiver device (Rx) address
    virtual const Address *getPeerAddress() = 0; // remote device (Tx) address
    virtual RFChannel getRFChannel() = 0;
    virtual void setRFChannel(RFChannel ch) = 0;
    virtual bool receive(RequestPacket *packet) = 0;
    virtual void send(const ResponsePacket *packet) = 0;
    virtual bool pair() = 0;
    virtual bool isPaired() = 0;
};

#endif // LOWCOSTRC_RX_H
// vim:et:sw=2:ai
