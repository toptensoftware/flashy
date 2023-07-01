# Override ARCHFLAGS as CPU not setup for all instructions sets
# Also define BOOTLOADER so vector.S inserts space for loaded program
ARCHFLAGS=-DBOOTLOADER

# Build relocated image
default_target: relocated_image

# Pass flashy version from package.json to C code for the PING ack response
FLASHY_VERSION := $(shell node -p "require('../flashy/package.json').version")
COMMA := ,
DEFINE += FLASHY_VERSION=$(subst .,$(COMMA),$(FLASHY_VERSION)),0
TARGETSUFFIX=-himem

# Use hi-memory link script (save for aarch32 and 64)
LINKSCRIPT = link_script_himem

# Main make rules
include raspi.mk

# Create zip archive
bootloader.zip: cleanall
	@$(MAKE) aarch CONFIG=release
	rm -f ../flashy/bootloader.zip
	zip ../flashy/bootloader.zip -r -j . -i *.img

$(OUTDIR)/$(TARGETBASE).img: $(TARGET) relocstub/reloc.S
	@echo "  RELOC $(notdir $@)"
	$(Q)$(PREFIX)gcc -o $(@:%.img=%.o) -c relocstub/reloc.S -DKERNELIMAGEFILE="$(TARGET)" -DAARCH=$(AARCH)
	$(Q)$(PREFIX)ld $(@:%.img=%.o) -T link_script_$(AARCH) --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	$(Q)$(PREFIX)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	$(Q)$(PREFIX)objcopy $(@:%.img=%.elf) -O binary $@
	$(Q)cp $@ ../flashy/bootloader_images/

relocated_image: $(OUTDIR)/$(TARGETBASE).img

