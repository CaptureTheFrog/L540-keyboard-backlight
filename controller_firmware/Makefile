# Makefile for programming ATtiny85 with avr-gcc and avrdude

# Set the programming tool and port
PROGRAMMER = your_programmer
PORT = /dev/ttyUSB0

# Set the source file name (without extension)
SOURCE_FILE = main

# Set the target file name (without extension)
TARGET = controller_firmware

# Set the microcontroller type
MCU = attiny85

# Compiler and flags
CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS = -g -Os -mmcu=$(MCU)

# Default target
all: $(TARGET).hex

# Compile C code to hex file
$(TARGET).hex: $(SOURCE_FILE).c
	$(CC) $(CFLAGS) -o $(TARGET).elf $^
	$(OBJCOPY) -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex

# Program the microcontroller
program: $(TARGET).hex
	micronucleus --run $(TARGET).hex

# Verify the programming
verify:
	avrdude -c $(PROGRAMMER) -p $(MCU) -P $(PORT) -U flash:r:read_back.hex:i

# Clean up temporary files
clean:
	rm -f $(TARGET).elf $(TARGET).hex read_back.hex
	
# Run simavr
simulate: $(TARGET).hex
	simavr -f 16500000 -m $(MCU) -v -t $(TARGET).elf


# Flash the microcontroller and verify
flash: program verify

.PHONY: all program verify clean flash
