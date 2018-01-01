ARMGNU ?= arm-none-eabi

FLAGS := -march=armv8-a -mfpu=neon-vfpv4 -mtune=cortex-a8
CFLAGS := -Wall -ffreestanding $(FLAGS)

# The intermediate directory for compiled object files.
BUILD = 
# The directory in which source files are stored.
SOURCE = source/
# The directory in which header files are stored.
INCLUDE1 = include/ 
INCLUDE2 = /opt/uARM/include/uarm 

OBJECTS := $(patsubst $(SOURCE)%.c,$(BUILD)%.o,$(wildcard $(SOURCE)*.c))

TARGET := p1test.elf

all: $(TARGET)

# Rule to make the elf file.
$(TARGET) : $(OBJECTS)
	$(ARMGNU)-gcc -nostartfiles -Wl,-r $(OBJECTS) -o $(TARGET)

# Rule to make the object files.
$(BUILD)%.o: $(SOURCE)%.c $(BUILD)
	$(ARMGNU)-gcc $(CFLAGS) -c -I $(INCLUDE1) -I $(INCLUDE2) -g $< -o $@

clean:
	rm $(TARGET) $(BUILD)*.o
