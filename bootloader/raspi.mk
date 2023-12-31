CONFIG ?= release

# Crack platform
ifeq ($(strip $(PLATFORM)),rpi1)
RASPI=1
AARCH=32
KERNELNAME=kernel
ARCHFLAGS?=-mcpu=arm1176jzf-s -marm -mfpu=vfp -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi2)
RASPI=2
AARCH=32
KERNELNAME=kernel7
ARCHFLAGS?=-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi3)
RASPI=3
AARCH=32
KERNELNAME=kernel8-32
ARCHFLAGS?=-mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi4)
RASPI=4
AARCH=32
KERNELNAME=kernel7l
ARCHFLAGS?=-mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi3-64)
RASPI=3
AARCH=64
KERNELNAME=kernel8
ARCHFLAGS?=-mcpu=cortex-a53 -mlittle-endian
else ifeq ($(strip $(PLATFORM)),rpi4-64)
RASPI=4
AARCH=64
KERNELNAME=kernel8-rpi4
ARCHFLAGS?=-mcpu=cortex-a72 -mlittle-endian
else
$(error Unknown platform '$(PLATFORM)')
endif


# Tool chain
ifeq ($(OS),Windows_NT)
ARMTOOLS ?= Z:/armtools/
ARMTOOLS_PLATFORM ?= mingw-w64-i686
else
ARMTOOLS ?= ~/armtools/
ARMTOOLS_PLATFORM ?= x86_64
endif
ARMTOOLS_VERSION=12.2.rel1
ifeq ($(AARCH),32)
PREFIX ?= $(ARMTOOLS)/arm-gnu-toolchain-$(ARMTOOLS_VERSION)-$(ARMTOOLS_PLATFORM)-arm-none-eabi/bin/arm-none-eabi-
STRICTALIGNFLAG := -mno-unaligned-access
else ifeq ($(AARCH),64)
PREFIX ?= $(ARMTOOLS)/arm-gnu-toolchain-$(ARMTOOLS_VERSION)-$(ARMTOOLS_PLATFORM)-aarch64-none-elf/bin/aarch64-none-elf-
STRICTALIGNFLAG := -mstrict-align
else
$(error Unknown AARCH: '$(AARCH)')
endif
MAKEFLAGS += --no-print-directory

# Strict align?
STRICTALIGN ?= 1
ifeq ($(strip $(STRICTALIGN)),1)
ARCHFLAGS += $(STRICTALIGNFLAG)
endif

# C Options
GCC_COMMONFLAGS += -nostdlib -nostartfiles -ffreestanding $(ARCHFLAGS)
DEFINE += AARCH=$(AARCH) AARCH$(AARCH) RASPI=$(RASPI) RASPI$(RASPI)

# Project Kind
PROJKIND ?= custom
HEXFILE = $(TARGET:%.img=%.hex)
ELFFILE = $(TARGET:%.img=%.elf)
LIBGCC	  := $(shell $(PREFIX)gcc $(ARCHFLAGS) -print-file-name=libgcc.a)
EXTRALIBS += $(LIBGCC)
LINKSCRIPT ?= link_script_$(AARCH)

ifeq ($(strip $(PROJKIND)),custom)
TARGETBASE?=$(KERNELNAME)
TARGETNAME?=$(TARGETPREFIX)$(TARGETBASE)$(TARGETSUFFIX).img
endif

# Main Rules
TOOLCHAIN = gcc
RULESHOME ?= ../Rules
include $(RULESHOME)/Rules.mk

ifeq ($(strip $(PROJKIND)),custom)

# Link
$(ELFFILE): $(PRECOMPILE_TARGETS) $(OBJS) $(LINKPROJECTLIBS) 
	@echo "  LD    $(notdir $@)"
	$(Q)$(PREFIX)ld -o $@ $^ $(LIBS) $(GCC_LIBS) $(EXTRALIBS) --no-warn-rwx-segments -T $(LINKSCRIPT) -Map $(@:%.elf=%.map) -o $@
	$(Q)$(PREFIX)objdump -D $@ > $(@:%.elf=%.lst)

# .img
$(TARGET) : $(ELFFILE)
	@echo "  COPY  $(notdir $@)"
	$(Q)$(PREFIX)objcopy $< -O binary $@

# .hex
$(HEXFILE): $(TARGET)
	@echo "  COPY  $(notdir $@)"
	$(Q)$(PREFIX)objcopy $(<:%.img=%.elf) -O ihex $@

# 32-bit targets
aarch32:
	$(Q)$(MAKE) PLATFORM=rpi1
	$(Q)$(MAKE) PLATFORM=rpi2
	#$(Q)$(MAKE) PLATFORM=rpi3
	$(Q)$(MAKE) PLATFORM=rpi4

# 64-bit targets
aarch64:
	$(Q)$(MAKE) PLATFORM=rpi3-64
	$(Q)$(MAKE) PLATFORM=rpi4-64

# 32 and 64-bit targets
aarch: aarch32 aarch64

# Uber clean
cleanall:
	@rm -rf bin

endif