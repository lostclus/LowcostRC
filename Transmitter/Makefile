BOARD=arduino:avr:pro:cpu=8MHzatmega328
PORT=/dev/ttyUSB0
EXTRA_FLAGS=
WITH_CONSOLE=
WITH_ADAFRUIT_SSD1306=1
WITH_SSD1306_ASCII=
FLAT_MENU=

ifeq ($(WITH_CONSOLE),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DWITH_CONSOLE
endif
ifeq ($(WITH_ADAFRUIT_SSD1306),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DWITH_ADAFRUIT_SSD1306
endif
ifeq ($(WITH_SSD1306_ASCII),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DWITH_SSD1306_ASCII
endif
ifeq ($(FLAT_MENU),1)
EXTRA_FLAGS := $(EXTRA_FLAGS) -DFLAT_MENU
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

monitor:
	minicom -D $(PORT)
