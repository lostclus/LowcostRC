BOARD=arduino:avr:micro
PORT=/dev/ttyACM0

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
