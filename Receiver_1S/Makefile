BOARD=arduino:avr:pro:cpu=8MHzatmega328
PORT=/dev/ttyUSB0

compile:
	arduino-cli compile \
		-b "$(BOARD)" \
		.

upload:
	arduino-cli upload \
		-v \
		-b "$(BOARD)" \
		-p $(PORT) \
		.

install: compile upload
