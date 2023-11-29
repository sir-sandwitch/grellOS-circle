
# Compiler and linker options
CC = aarch64-linux-gnu-gcc
LD = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy
CFLAGS = -nostdlib -nostartfiles

# Source file and output file
SRC = helloworld.s
OBJ = helloworld.o
ELF = helloworld.elf
BIN = helloworld.bin

all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

$(ELF): $(OBJ)
	$(LD) -T linker.ld -o $(ELF) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ) $(ELF) $(BIN)
