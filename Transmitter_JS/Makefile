BOARD=arduino:avr:uno
PORT=/dev/ttyUSB0
EXTRA_FLAGS=
WITH_CONSOLE=
PEER_ADDR=0x00,0x00,0x00,0x00,0x00,0x00
BATTERY_LOW_MV=3400

ifeq ($(WITH_CONSOLE),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DWITH_CONSOLE
endif

ifneq ($(PEER_ADDR),)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DPEER_ADDR=$(PEER_ADDR)
endif

ifneq ($(BATTERY_LOW_MV),)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DBATTERY_LOW_MV=$(BATTERY_LOW_MV)
endif

compile:
	arduino-cli compile \
		-b "$(BOARD)" \
		--build-property build.extra_flags="$(EXTRA_FLAGS)" \
		.

upload:
	arduino-cli upload \
		-b "$(BOARD)" \
		-p $(PORT) \
		.

install: compile upload

monitor:
	minicom -D $(PORT)
