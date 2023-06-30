# Override ARCHFLAGS as CPU not setup for all instructions sets
# Also define BOOTLOADER so vector.S inserts space for loaded program
ARCHFLAGS=-DBOOTLOADER


# Pass flashy version from package.json to C code for the PING ack response
FLASHY_VERSION := $(shell node -p "require('../flashy/package.json').version")
COMMA := ,
DEFINE += FLASHY_VERSION=$(subst .,$(COMMA),$(FLASHY_VERSION)),0

# Main make rules
include raspi.mk

# Create zip archive
bootloader.zip:
	@$(MAKE) aarch CONFIG=release
	rm -f ../flashy/bootloader.zip
	zip ../flashy/bootloader.zip -r -j . -i *.img
