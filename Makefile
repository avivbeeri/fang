SRC := src
OBJ := obj

CFLAGS := -g -Wall -std=c99

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

.phony: clean


fcc: $(OBJECTS)
	$(CC) $^ $(CFLAGS) -o $@ -fsanitize=address

test: file.o
	ld -o test file.o \
        -lSystem \
        -syslibroot `xcrun -sdk macosx --show-sdk-path` \
        -e _start \
        -arch arm64
file.o: file.S
	as -o file.o file.S
file.S: fcc test.f
	./fcc 

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -I$(SRC) -c $< $(CFLAGS) -o $@

clean:
	rm obj/*.o
	rm fcc
