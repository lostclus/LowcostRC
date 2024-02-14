BOARD=arduino:avr:pro:cpu=8MHzatmega328
PORT=/dev/ttyUSB0
EXTRA_FLAGS=
WITH_CONSOLE=

ifeq ($(WITH_CONSOLE),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DWITH_CONSOLE
endif

compile:
	arduino-cli compile \
		-b "$(BOARD)" \
		--build-property build.extra_flags="$(EXTRA_FLAGS)" \
		.

upload:
	arduino-cli upload \
		-v \
		-b "$(BOARD)" \
		-p $(PORT) \
		.

install: compile upload
