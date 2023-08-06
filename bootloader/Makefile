PLATFORM ?= rpi3-64

# Override ARCHFLAGS as CPU not setup for all instructions sets
ARCHFLAGS=-DBOOTLOADER

# Build relocated image
default_target: sub-projects relocated_image

# Pass flashy version from package.json to C code for the PING ack response
FLASHY_VERSION := $(subst -alpha,,$(shell node -p "require('../flashy/package.json').version"))
COMMA := ,
DEFINE += FLASHY_VERSION=$(subst .,$(COMMA),$(FLASHY_VERSION))
TARGETSUFFIX=-himem

INCLUDEPATH += ../lib/FFsh/FFsh/ff15/source
LINKPROJECTS = ../lib/FFsh ../lib/ceelib

# Use hi-memory link script (save for aarch32 and 64)
LINKSCRIPT = link_script_himem

# Main make rules
include raspi.mk

$(OUTDIR)/$(TARGETBASE).img: $(TARGET) relocstub/reloc.S
	@echo "  RELOC $(notdir $@)"
	$(Q)$(PREFIX)gcc -o $(@:%.img=%.o) -c relocstub/reloc.S -DKERNELIMAGEFILE="$(TARGET)" -DAARCH=$(AARCH)
	$(Q)$(PREFIX)ld $(@:%.img=%.o) -T link_script_$(AARCH) --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	$(Q)$(PREFIX)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	$(Q)$(PREFIX)objcopy $(@:%.img=%.elf) -O binary $@
	$(Q)cp $@ ../flashy/bootloader_images/

relocated_image: sub-projects $(OUTDIR)/$(TARGETBASE).img


flash: relocated_image
	flashy COM3 $(OUTDIR)/$(TARGETBASE).img