SRC := src
OBJ := obj

CFLAGS := -g -Wall

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

.phony: clean


fcc: $(OBJECTS)
	$(CC) $^ $(CFLAGS) -o $@ -fsanitize=address

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -I$(SRC) -c $< $(CFLAGS) -o $@

clean:
	rm obj/*.o
	rm fcc
