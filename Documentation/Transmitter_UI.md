# Transmitter User Interface
## Buttons

There are 3 buttons to interact with the user interface.

Screen button short press
: Go to the next screen

Screen button long press
: Go to the blank screen

Plus button
: Increase parameter value or confirm operation

Minus button
: Decrease parameter value or cancel operation

## Screens

Blank screen (default)
: Nothing displayed. Pressing the plus button will transmit user command 1.
Pressing the minus button will transmit user command 2.

Display screen
: Display profile name, transmitter battery voltage, receiver battery voltage and
link quality.

Profile
: Change current profile.

Profile name
: Edit profile name.

Radio / paired
: Display pair (bind) status. Pressing the plus button will start pairing
(binding) procedure. Pressing the minus button will unpair device.

Radio / peer
: Display/edit peer address

Radio / RF channel
: Set radio channel. For NRF24L01 value range is [0..125]. For ESP8266 range is
[0..11]. The `0` value means default channel (76 for NRF24L01 and 11 for
ESP8266)

Radio / PA level
: Set power amplifier level. For NRF24L01 value range is [1..4]. For ESP8266
there is only 1 PA level

Controls / J centers
: Set joysticks center

Controls / D/R A or B, X or Y
: Set axis double rate [10..1500]

Controls / Tr A or B, X or Y
: Set axis trimming [-1500..1500]

Controls / Invert A or B, X or Y
: Set axis value inversion [y,n]

Controls / Low SW 1, 2, 3, 4
: Set low value for switch [0..3000]

Controls / High SW 1, 2, 3, 4
: Set high value for switch [0..3000]

Mapping / Channel A or B, X or Y
: Map joystick axis to corresponding channel [1..8]

Mapping / Channel SW 1, 2, 3, 4
: Map switch to corresponding channel [1..8]

Peer / Bat low
: Set low Rx battery voltage threshold for alerting [0.1..20]

Peer / Save failsafe?
: Pressing the plus button will transmit a command that cause receiver to
remember current channels state and use it in failsafe mode (when no radio link
detected by receiver)

Save?
: Pressing the plus button will save settings of current profile in EEPROM

## Buzzer signals

1 very short high pitch signal
: Low link quality alert

1 short low pitch signal
: Link quality returned to normal

3 long low pitch signals
: Low battery alert
