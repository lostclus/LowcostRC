#ifndef LowcostRC_SPI_h
#define LowcostRC_SPI_h

#include <stdint.h>

const size_t SPI_PACKET_SIZE = 32;

const uint32_t STATUS_OK = 0;
const uint32_t STATUS_FAILURE = 1;
const uint32_t STATUS_STARTING = 101;
const uint32_t STATUS_TRANSMITING = 102;
const uint32_t STATUS_TELEMETRY = 103;
const uint32_t STATUS_SET_RF_CHANNEL = 200;

#endif
