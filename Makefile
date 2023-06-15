CC := clang
SRC := src
OBJ := obj
TEST := test
CFLAGS := -g -Wall -std=c99 -Wno-error=switch -Wno-error=unused-variable  -Wno-error=unused-function

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))
DEP = $(OBJECTS:%.o=%.d)

TESTS := $(wildcard $(TEST)/*.c)
TESTBINS := $(patsubst $(TEST)/%.c, $(TEST)/bin/%, $(TESTS))
 
.phony: clean test check


fgcc: $(OBJECTS)
	$(CC) $^ $(CFLAGS) -o $@ -fsanitize=address

example: file.o
	ld -o example file.o \
        -lSystem \
        -syslibroot `xcrun -sdk macosx --show-sdk-path` \
        -e _start \
        -arch arm64
file.o: file.S
	as -g -o file.o file.S
file.S: fgcc example.fg
	./fgcc 

check: $(OBJECTS) fgcc
	./check.sh

test: $(TEST)/bin $(TESTBINS) $(OBJECTS) fgcc
	for test in $(TESTBINS) ; do ./$$test --verbose ; done
	./check.sh

$(TEST)/bin/%: $(TEST)/%.c
	$(CC) $(shell pkg-config --cflags --libs criterion) -I$(SRC) $(CFLAGS) $< $(OBJS) -o $@ -lcriterion 

$(TEST)/bin:
	mkdir -p $@

# Include all .d files
-include $(DEP)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -I$(SRC) -c  -MMD $< $(CFLAGS) -o $@

clean:
	rm $(OBJECTS)
	rm fgcc
	rm $(TESTBINS)
	rm example
