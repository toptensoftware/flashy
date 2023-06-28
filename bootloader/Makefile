# Override ARCHFLAGS as CPU not setup for all instructions sets
# Also define BOOTLOADER so vector.S inserts space for loaded program
ARCHFLAGS=-DBOOTLOADER

# Main make rules
include raspi.mk

# Create zip archive
bootloader.zip:
	@$(MAKE) aarch CONFIG=release
	zip ../flashy/bootloader.zip -r -j . -i *.img
