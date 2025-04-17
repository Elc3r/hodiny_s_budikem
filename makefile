# Definuj mikrokontrolér a frekvenci
MCU = atmega32a
F_CPU = 16000000UL

# Nástroje
CC = avr-gcc
OBJCOPY = avr-objcopy
RM = rm -rf
MKDIR = mkdir -p

# Kompilátorové příznaky
CFLAGS = -Wall -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU)
LDFLAGS = -mmcu=$(MCU)

# Soubory
TARGET = main
C_SRCS = main.cpp

# Výstupy
OBJS = Debug/$(notdir $(C_SRCS:.cpp=.o))
HEX = Debug/$(TARGET).hex
ELF = Debug/$(TARGET).elf

# Překlad
all: | Debug $(HEX)

Debug:
	$(MKDIR) $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(ELF): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

Debug/%.o: %.cpp | Debug
	$(CC) $(CFLAGS) -c $< -o $@

# Úklid
clean:
	$(RM) Debug/*.o $(ELF) $(HEX)
