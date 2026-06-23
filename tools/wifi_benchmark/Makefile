TARGET := nocturne-wifi-test
BUILD := build
SOURCE := source
ROOT := ../..

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM

CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
SMDHTOOL := $(DEVKITPRO)/tools/bin/smdhtool
MAKEROM := $(DEVKITPRO)/tools/bin/makerom

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := $(ARCH) -O2 -Wall -Wextra -std=gnu17 -ffunction-sections \
	-I$(DEVKITPRO)/libctru/include
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,--gc-sections \
	-L$(DEVKITPRO)/libctru/lib
LIBS := -lctru -lm

.PHONY: all clean

all: $(TARGET).cia

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: $(SOURCE)/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(BUILD)/main.o
	$(CC) $(LDFLAGS) $< $(LIBS) -o $@

$(BUILD)/$(TARGET).smdh: $(ROOT)/cia_stuff/icon.png | $(BUILD)
	$(SMDHTOOL) --create "Nocturne Wi-Fi Lab" "Network-only 3DS throughput test" "p0mpurin" $< $@

$(BUILD)/benchmark.rsf: $(ROOT)/cia_stuff/3hs.rsf | $(BUILD)
	sed -e 's/Title                   : 3hs_app/Title                   : nocturne_wifi_test/' \
	    -e 's/ProductCode             : CTR-N-3HSA/ProductCode             : CTR-N-NWFT/' \
	    -e 's/UniqueId                : 0x3DF10/UniqueId                : 0x3DF11/' \
	    $< > $@

$(TARGET).cia: $(TARGET).elf $(BUILD)/$(TARGET).smdh $(BUILD)/benchmark.rsf
	cp $(ROOT)/cia_stuff/banner.bnr $(BUILD)/$(TARGET).bnr
	$(MAKEROM) -f cia -o $@ -target t -elf $(TARGET).elf \
		-icon $(BUILD)/$(TARGET).smdh -banner $(BUILD)/$(TARGET).bnr \
		-rsf $(BUILD)/benchmark.rsf -DROMFS_PATH="$(ROOT)/romfs" -ver 1

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).cia
