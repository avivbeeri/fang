SRC := src
OBJ := obj
TEST := test
CFLAGS := -g -Wall -std=c99 -Werror -Wno-error=switch -Wno-error=unused-variable -ferror-limit=1 -Wno-error=unused-function

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

TESTS := $(wildcard $(TEST)/*.c)
TESTBINS := $(patsubst $(TEST)/%.c, $(TEST)/bin/%, $(TESTS))
 
.phony: clean test


fgcc: $(OBJECTS)
	$(CC) $^ $(CFLAGS) -o $@ -fsanitize=address

example: file.o
	ld -o example file.o \
        -lSystem \
        -syslibroot `xcrun -sdk macosx --show-sdk-path` \
        -e _start \
        -arch arm64
file.o: file.S
	as -o file.o file.S
file.S: fgcc example.fg
	./fgcc 

test: $(TEST)/bin $(TESTBINS) $(OBJECTS)
	for test in $(TESTBINS) ; do ./$$test --verbose ; done

$(TEST)/bin/%: $(TEST)/%.c
	$(CC) $(shell pkg-config --cflags --libs criterion) -I$(SRC) $(CFLAGS) $< $(OBJS) -o $@ -lcriterion 

$(TEST)/bin:
	mkdir -p $@


$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -I$(SRC) -c $< $(CFLAGS) -o $@

clean:
	rm $(OBJECTS)
	rm fgcc
	rm $(TESTBINS)
	rm example
