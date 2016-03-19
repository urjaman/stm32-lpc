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
CFLAGS		+= -Os -fno-common -flto
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
OBJS		+= stm32-lpc.o usbcdc.o extuart.o

.SUFFIXES: .elf .bin .hex
.SECONDEXPANSION:
.SECONDARY:

all: elf size
elf: $(BINARY).elf
bin: $(BINARY).bin


$(BINARY).elf:	$(OBJS) $(LDSCRIPT) $(LIB_DIR)/lib$(LIBNAME).a
	$(Q)$(LD) $(LDFLAGS) $(ARCH_FLAGS) $(OBJS) $(LDLIBS) -o $(*).elf

%.bin: %.elf
	$(Q)$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCH_FLAGS) -o $(*).o -c $(*).c

$(LIB_DIR)/lib$(LIBNAME).a:
	cd $(OPENCM3_DIR) && $(MAKE) DEBUG_FLAGS="-flto"

program: bin
	dfu-util -a 0 --dfuse-address 0x08000000:leave -D $(BINARY).bin

clean:
	rm -f $(BINARY).elf *.bin *.o

objdump: elf
	$(OBJDUMP) -xdC $(BINARY).elf | less

size: elf
	$(PREFIX)-size $(BINARY).elf

.PHONY: clean elf bin hex
