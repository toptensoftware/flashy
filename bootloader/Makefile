
ifeq ($(OS),Windows_NT)
ARMTOOLS ?= Z:/armtools/
ARMTOOLS_PLATFORM=mingw-w64-i686
else
ARMTOOLS ?= ~/armtools/
ARMTOOLS_PLATFORM=x86_64
endif

ARMTOOLS_VERSION=12.2.rel1

PREFIX ?= $(ARMTOOLS)/arm-gnu-toolchain-$(ARMTOOLS_VERSION)-$(ARMTOOLS_PLATFORM)-arm-none-eabi/bin/arm-none-eabi-
PREFIX64 ?= $(ARMTOOLS)/arm-gnu-toolchain-$(ARMTOOLS_VERSION)-$(ARMTOOLS_PLATFORM)-aarch64-none-elf/bin/aarch64-none-elf-
FLASHBAUD ?= 115200

COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding -DDEFAULTBAUD=$(FLASHBAUD)
COPS += -MD -MF $(@:%.o=%.d) -MT $@  -MP 

SRCFILES = $(filter-out periph.c,$(wildcard *.c))
OBJS = $(SRCFILES:%.c=%.o)
OBJS64 = $(addsuffix _64.o,$(basename $(OBJS)))

IMAGES = kernel.img kernel7.img kernel7l.img 
IMAGES64 = kernel8.img kernel8-rpi4.img 

all: bootloader.zip

aarch32: $(IMAGES)

aarch64: $(IMAGES64)

clean:
	@echo " CLEAN "`pwd`
	@rm -f *.img
	@rm -f *.o
	@rm -f *.elf
	@rm -f *.lst
	@rm -f *.d
	@rm -f *.map
	@rm -f *.zip

bootloader.zip: $(IMAGES) $(IMAGES64)
	zip bootloader.zip *.img
	cp bootloader.zip ../flashy/

-include $(wildcard *.d)

%.o : %.c 
	@echo "  GCC  "$@
	@$(PREFIX)gcc $(COPS) -c $< -o $@ -DAARCH=32

%_64.o : %.c
	@echo "  GCC  "$@
	@$(PREFIX64)gcc $(COPS) -c $< -o $@ -DAARCH=64

vectors.o : vectors.s
	@echo "  AS   "$@
	@$(PREFIX)as vectors.s -o vectors.o

vectors64.o : vectors64.s
	@echo "  AS   "$@
	@$(PREFIX64)as vectors64.s -o $@

periph.o : periph.c BCM2835.h
	@echo "  GCC  "$@
	@$(PREFIX)gcc $(COPS) -c periph.c -o $@ -DAARCH=32

periph7.o : periph.c BCM2836.h
	@echo "  GCC  "$@
	@$(PREFIX)gcc $(COPS) -c periph.c -o $@ -DRPI2 -DAARCH=32

periph7_rpi4.o : periph.c BCM2711.h
	@echo "  GCC  "$@
	@$(PREFIX)gcc $(COPS) -c periph.c -o $@ -DRPI4 -DAARCH=32

periph64.o : periph.c BCM2836.h
	@echo "  GCC  "$@
	@$(PREFIX64)gcc $(COPS) -c periph.c -o $@ -DAARCH=64

periph64_rpi4.o : periph.c BCM2711.h
	@echo "  GCC  "$@
	@$(PREFIX64)gcc $(COPS) -c periph.c -o $@ -DRPI4 -DAARCH=64
	

kernel.img : loader vectors.o periph.o $(OBJS)
	@echo "  LD   "$@
	@$(PREFIX)ld vectors.o periph.o $(OBJS) -T loader --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX64)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX64)objcopy $(@:%.img=%.elf) -O binary $@

kernel7.img : loader vectors.o periph7.o $(OBJS) 
	@echo "  LD   "$@
	@$(PREFIX)ld vectors.o periph7.o $(OBJS) -T loader --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX64)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX64)objcopy $(@:%.img=%.elf) -O binary $@

kernel7l.img : loader vectors.o periph7_rpi4.o $(OBJS)
	@echo "  LD   "$@
	@$(PREFIX)ld vectors.o periph7_rpi4.o $(OBJS) -T loader --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX64)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX64)objcopy $(@:%.img=%.elf) -O binary $@

kernel8.img : loader64 vectors64.o periph64.o $(OBJS64)
	@echo "  LD   "$@
	@$(PREFIX64)ld vectors64.o periph64.o $(OBJS64) -T loader64 --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX64)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX64)objcopy $(@:%.img=%.elf) -O binary $@

kernel8-rpi4.img : loader64 vectors64.o periph64_rpi4.o $(OBJS64)
	@echo "  LD   "$@
	@$(PREFIX64)ld vectors64.o periph64_rpi4.o $(OBJS64) -T loader64 --no-warn-rwx-segments -Map $(@:%.img=%.map) -o $(@:%.img=%.elf)
	@$(PREFIX64)objdump -D $(@:%.img=%.elf) > $(@:%.img=%.lst)
	@$(PREFIX64)objcopy $(@:%.img=%.elf) -O binary $@
