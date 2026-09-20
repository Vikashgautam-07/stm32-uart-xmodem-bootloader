# Makefile for STM32F103C8T6 bare-metal bring-up project

TARGET      = firmware
BUILD_DIR   = build

PREFIX      = arm-none-eabi
CC          = $(PREFIX)-gcc
OBJCOPY     = $(PREFIX)-objcopy
SIZE        = $(PREFIX)-size
GDB         = gdb-multiarch

CPU_FLAGS   = -mcpu=cortex-m3 -mthumb
CFLAGS      = $(CPU_FLAGS) -Wall -O0 -g3 -ffreestanding -nostdlib -fno-builtin
LDFLAGS     = $(CPU_FLAGS) -T stm32f103.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

SRCS        = src/startup_stm32f103.s src/main.c src/uart.c
OBJS        = $(BUILD_DIR)/startup_stm32f103.o \
			  $(BUILD_DIR)/main.o \
			  $(BUILD_DIR)/uart.o

.PHONY: all clean flash debug openocd size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin size

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/startup_stm32f103.o: src/startup_stm32f103.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: src/main.c src/uart.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uart.o: src/uart.c src/uart.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

# Flash via OpenOCD (run in one terminal; make sure ST-Link + board are attached)
flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
		-c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

# Start OpenOCD only (leave running, connect GDB from another terminal)
openocd:
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

# Start GDB and auto-connect to a running OpenOCD session on port 3333
debug: $(BUILD_DIR)/$(TARGET).elf
	$(GDB) $(BUILD_DIR)/$(TARGET).elf -ex "target extended-remote localhost:3333"

clean:
	rm -rf $(BUILD_DIR)
