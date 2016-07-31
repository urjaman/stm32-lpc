# What are we...
BINARY = stm32-lpc
# Target stuff
LDSCRIPT	= ./libopencm3/lib/stm32/f0/stm32f04xz6.ld
LIBNAME		= opencm3_stm32f0
DEFS		+= -DSTM32F0
FP_FLAGS	?= -msoft-float
ARCH_FLAGS	= -mthumb -mcpu=cortex-m0 $(FP_FLAGS)
# Compiler stuff
PREFIX		?= arm-none-eabi
CC		:= $(PREFIX)-gcc
CXX		:= $(PREFIX)-g++
LD		:= $(PREFIX)-gcc
AR		:= $(PREFIX)-ar
AS		:= $(PREFIX)-as
OBJCOPY		:= $(PREFIX)-objcopy
OBJDUMP		:= $(PREFIX)-objdump
# OpenCM3 dirs
OPENCM3_DIR	:= ./libopencm3
INCLUDE_DIR	= $(OPENCM3_DIR)/include
LIB_DIR		= $(OPENCM3_DIR)/lib
# CFLAGS
CFLAGS		+= -Os -fno-common -flto -flto-partition=none -I.
CFLAGS		+= -Wextra -Wshadow -Wimplicit-function-declaration -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes -Wno-main
# C++ Flags ...
CXXFLAGS	= $(CFLAGS)
# C preprocessor stuff
CPPFLAGS	+= -Wall -Wundef
CPPFLAGS	+= -I$(INCLUDE_DIR) $(DEFS)
# LD stuff
LDFLAGS		+= --static -nostartfiles -L$(LIB_DIR) -T$(LDSCRIPT) -Wl,--gc-sections
LDLIBS		+= -l$(LIBNAME) -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

# Our actual source files...
SOURCES		= stm32-lpc.c usbcdc.c extuart.c delay.c flash.c debug.c mutex.c dmacopy.c usbopt.c
DEPS		= extuart.h flash.h main.h uart.h usbcdc.h debug.h mutex.h dmacopy.h usbopt.h

#libfrser
include	libfrser/Makefile.frser
include	libfrser/Makefile.lpcfwh

.SUFFIXES: .elf .bin .hex
.SECONDEXPANSION:
.SECONDARY:

all: elf size
elf: $(BINARY).elf
bin: $(BINARY).bin


$(BINARY).elf:	$(SOURCES) $(DEPS) $(LDSCRIPT) $(LIB_DIR)/lib$(LIBNAME).a
	$(Q)$(CC) $(LDFLAGS) $(ARCH_FLAGS) $(CFLAGS) $(CPPFLAGS) $(SOURCES) $(LDLIBS) -o $(*).elf

%.bin: %.elf
	$(Q)$(OBJCOPY) -Obinary $(*).elf $(*).bin

$(LIB_DIR)/lib$(LIBNAME).a:
	cd $(OPENCM3_DIR) && $(MAKE) DEBUG_FLAGS="-flto"

program: bin
	dfu-util -a 0 --dfuse-address 0x08000000:leave -D $(BINARY).bin

clean:
	rm -f $(BINARY).elf *.bin *.o libfrser/*.o

objdump: elf
	$(OBJDUMP) -xdC $(BINARY).elf | less

size: elf
	$(PREFIX)-size $(BINARY).elf

.PHONY: clean elf bin hex
