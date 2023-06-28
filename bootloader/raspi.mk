PLATFORM ?= rpi2
CONFIG ?= release

# Crack platform
ifeq ($(strip $(PLATFORM)),rpi1)
RASPI=1
AARCH=32
TARGETNAME=kernel.img
#ARCHFLAGS=-mcpu=arm1176jzf-s -marm -mfpu=vfp -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi2)
RASPI=2
AARCH=32
TARGETNAME=kernel7.img
#ARCHFLAGS=-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi3)
RASPI=3
AARCH=32
TARGETNAME=kernel8-32.img
#ARCHFLAGS=-mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi4)
RASPI=4
AARCH=32
TARGETNAME=kernel7l.img
#ARCHFLAGS=-mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
else ifeq ($(strip $(PLATFORM)),rpi3-64)
RASPI=3
AARCH=64
TARGETNAME=kernel8.img
#ARCHFLAGS=-mcpu=cortex-a53 -mlittle-endian
else ifeq ($(strip $(PLATFORM)),rpi4-64)
RASPI=4
AARCH=64
TARGETNAME=kernel8-rpi4.img
#ARCHFLAGS=-mcpu=cortex-a72 -mlittle-endian
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
else ifeq ($(AARCH),64)
PREFIX ?= $(ARMTOOLS)/arm-gnu-toolchain-$(ARMTOOLS_VERSION)-$(ARMTOOLS_PLATFORM)-aarch64-none-elf/bin/aarch64-none-elf-
else
$(error Unknown AARCH: '$(AARCH)')
endif
MAKEFLAGS += --no-print-directory

# C Options
GCC_COMMONFLAGS = -nostdlib -nostartfiles -ffreestanding $(ARCHFLAGS)
DEFINE += AARCH=$(AARCH) AARCH$(AARCH) RASPI=$(RASPI) RASPI$(RASPI)

# Project Kind
PROJKIND = custom
HEXFILE = $(TARGET:%.img=%.hex)
LIBGCC	  := "$(shell $(PREFIX)gcc $(ARCHFLAGS) -print-file-name=libgcc.a)"
EXTRALIBS += $(LIBGCC)
ifeq ($(AARCH),32)
LINKSCRIPT = link_script
else
LINKSCRIPT = link_script_64
endif

# Main Rules
TOOLCHAIN = gcc
include ../Rules/rules.mk

# Link
$(TARGET): $(PRECOMPILE_TARGETS) $(OBJS) $(LINKPROJECTLIBS)
	@echo "  LD    $(notdir $@)"
	@$(PREFIX)ld -o $@ $^ $(LIBS) $(GCC_LIBS) $(EXTRALIBS) -T $(LINKSCRIPT) --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX)objcopy $(@:%.img=%.elf) -O binary $@

# Hex file
$(HEXFILE): $(TARGET)
	@echo "  COPY  $(notdir $(HEXFILE))"
	@$(PREFIX)objcopy $(<:%.img=%.elf) -O ihex $@

# 32-bit targets
aarch32:
	@$(MAKE) PLATFORM=rpi1
	@$(MAKE) PLATFORM=rpi2
	@$(MAKE) PLATFORM=rpi3
	@$(MAKE) PLATFORM=rpi4

# 64-bit targets
aarch64:
	@$(MAKE) PLATFORM=rpi3-64
	@$(MAKE) PLATFORM=rpi4-64

# 32 and 64-bit targets
aarch: aarch32 aarch64

# Uber clean
cleanall:
	@rm -rf bin
