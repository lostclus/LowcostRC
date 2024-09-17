# LowcostRC

LowcostRC is low price remote control system based on Arduino.

<img src="Documentation/Images/Transmitter.jpg" width="300" height="200">

## Hardware

The following main components are used for the transmitter:

- Arduino Pro Mini as main MCU
- NRF24L01 or ESP8266 as radio module
- 0.96" SSD1306 OLED Display
- Two 2-axis joysticks

Receiver can be build using Arduino + NRF24L01 or a single ESP8266.

## Transmitter features
 
- 8 channels
- Pluggable radio modules (NRF24L01 or ESP8266)
- User interface with monochrome display and 3 buttons
- Multiple profiles (models) can be stored in EEPROM
- Various parameters can be changed like channels mapping, trimming, double rate, etc

## Directory structure

`LowcostRC_Core`
: Core library

`LowcostRC_Rx_ESP8266`
: ESP8266 receiver library

`LowcostRC_Rx_nRF24`:
NRF24L01 receiver library

`Transmitter`:
 Transmitter code

`Receiver_1S`:
Sample receiver sketch for plane. 1S power, NRF24L01 radio, brushed motor.

`Receiver_2S`:
Sample receiver sketch for plane. 2S power, NRF24L01 radio, brushless motor.

`Receiver_ESP8266`:
Sample receiver sketch for plane. 1S power, ESP8266 radio, brushed motor.

`Receiver_Sim`:
Sample receiver sketch to use with flight simulator on PC. Based on Arduino
Micro and NRF24L01. Arduino connects to PC using USB cable and recognized as
gamepad device with 8 axis.

`Documentation/Schematics`:
Schematics

To compile and flush you need Arduino IDE or Arduino CLI. In order for the
compiler can see the libraries you need to copy (or make symlinks)
`lowcostRC_*` directories to Arduino libraries directory.
