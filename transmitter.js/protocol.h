const byte MAGICK = 'p';
const byte RADIO_PIPE[6] = "pl2023";

struct {
  byte magick;
  int alerons;
  int elevator;
  byte engine;
} request;

struct {
    byte magick;
    int battaryMV;
} response;
