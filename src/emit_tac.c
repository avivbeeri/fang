/* MIT License

  Copyright (c) 2023 Aviv Beeri
  Copyright (c) 2015 Robert "Bob" Nystrom

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "options.h"
#include "tac.h"
PLATFORM p;

// ****** GB PLATFORM TEMP ******

static int* sizeTable = NULL;
static int TYPE_getSize(TYPE_ID id) {
  TYPE_ENTRY entry = TYPE_get(id);
  if (entry.entryType == ENTRY_TYPE_PRIMITIVE) {
    return sizeTable[id];
  }
  if (entry.entryType == ENTRY_TYPE_ARRAY) {
    // PTR
    return sizeTable[11];
  }

  if (entry.entryType != ENTRY_TYPE_RECORD) {
    return 8;
  }
  int size = 0;
  for (int i = 0; i < arrlen(entry.fields); i++) {
    if (entry.fields[i].elementCount == 0) {
      size += TYPE_getSize(entry.fields[i].typeIndex);
    } else {
      size += TYPE_getSize(TYPE_getParentId(entry.fields[i].typeIndex)) * entry.fields[i].elementCount;
    }
  }
  return size;
}


struct ACCESS_RECORD {
  TAC_OPERAND operand;
  bool local; // Global
  uint64_t start;
  uint64_t end;
};

static void GB_setup() {
  arrput(sizeTable, 0); // NULL
  arrput(sizeTable, 0); // VOID
  arrput(sizeTable, 1); // BOOL
  arrput(sizeTable, 1); // u8
  arrput(sizeTable, 1); // i8
  arrput(sizeTable, 2); // u16
  arrput(sizeTable, 2); // i16
  arrput(sizeTable, 2); // number
  arrput(sizeTable, 2); // string ptr
  arrput(sizeTable, 2); // fn ptr
  arrput(sizeTable, 1); // char
  arrput(sizeTable, 2); // ptr
}

static void GB_free() {
  arrfree(sizeTable);
}

static char* symbol(TAC_OPERAND op) {
  return NULL;
}


static void scanBlock(TAC_BLOCK* block) {
  TAC* instruction = block->start;
  uint64_t step = 0;
  struct ACTIVE_RECORD* records = NULL;
  while (instruction != NULL) {
    switch (instruction->tag) {
      case TAC_TYPE_COPY:
        {
          // find operand in records
          // if not exist, figure out of global, or create with start index,
          // else, record end index
          //
        }
        break;
    }
    instruction = instruction->next;
    step++;
  }
}

static void emitBlock(FILE* f, TAC_BLOCK* block) {
  TAC* instruction = block->start;
  while (instruction != NULL) {
    switch (instruction->tag) {
      case TAC_TYPE_COPY:
        {
          fprintf(f, "LD %s, %s", symbol(instruction->op1), symbol(instruction->op2));
        }
        break;
    }
    instruction = instruction->next;
  }
}

// ****** GB PLATFORM  END ******



static void emitProgram(FILE* f, TAC_PROGRAM program) {
  // get file
  for (int i = 0; i < arrlen(program.sections); i++) {
    TAC_SECTION section = program.sections[i];
    printf("SECTION \"code\", ROM0\n");
    for (int j = 0; j < arrlen(section.data); j++) {
      TAC_DATA data = section.data[j];
      if (data.module != EMPTY_STRING) {
        // printf("%s::", CHARS(data.module));
      }

      // printf("%s :  %s - %s\n", CHARS(data.name), CHARS(TYPE_get(data.type).name), data.constant ? "constant" : "variable");
    }
    for (int j = 0; j < arrlen(section.functions); j++) {
      TAC_FUNCTION fn = section.functions[j];

      printf("fn ");
      if (fn.module != EMPTY_STRING) {
        printf("%s::", CHARS(fn.module));
      }
      printf("%s(...)\n", CHARS(fn.name));

      TAC_BLOCK* block = fn.start;
      while (block != NULL) {
        // TODO: handle branched blocks
        emitBlock(f, block);
        block = block->next;
      }
    }
  }
}

void tacToAsm(TAC_PROGRAM program, PLATFORM platform) {
  p = platform;

  FILE* f = stdout;
  if (!options.toTerminal) {
    char* filename = options.outfile == NULL ? "file.S" : options.outfile;
    f = fopen(filename, "w");

    if (f == NULL)
    {
      printf("Error opening file!\n");
      exit(1);
    }
  }

  GB_setup();
  p.init();
  // Do something to print program
  emitProgram(f, program);
  p.complete();
  fprintf(f, "\n");
  if (!options.toTerminal) {
    fclose(f);
  } else {
    fflush(stdout);
  }

  PLATFORM_shutdown();
  GB_free();
//   EVAL_free();
}
