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

struct ACCESS_RECORD {
  TAC_OPERAND operand;
  bool local; // Global
  uint64_t start;
  uint64_t end;
};

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

static bool TAC_OPERAND_equal(TAC_OPERAND op1, TAC_OPERAND op2) {
  if (op1.tag == op2.tag) {
    if (op1.tag == TAC_OPERAND_TEMPORARY) {
      return op1.data.TAC_OPERAND_TEMPORARY.n == op2.data.TAC_OPERAND_TEMPORARY.n;
    } else if (op1.tag == TAC_OPERAND_VARIABLE) {
      struct TAC_OPERAND_VARIABLE v1 = op1.data.TAC_OPERAND_VARIABLE;
      struct TAC_OPERAND_VARIABLE v2 = op2.data.TAC_OPERAND_VARIABLE;
      return v1.module == v2.module && v1.name == v2.name && v1.scopeIndex == v2.scopeIndex;
    }
  }
  return false;
}

static struct ACCESS_RECORD* updateRecords(struct ACCESS_RECORD* records, uint64_t step, TAC_OPERAND operand) {

  if (operand.tag != TAC_OPERAND_TEMPORARY && operand.tag != TAC_OPERAND_VARIABLE) {
    return records;
  }
  int i = 0;
  bool found = false;
  for (; i < arrlen(records); i++) {
    struct ACCESS_RECORD current = records[i];
    if (current.operand.tag == TAC_OPERAND_LITERAL || current.operand.tag == TAC_OPERAND_LABEL) {
      continue;
    }
    if (TAC_OPERAND_equal(current.operand, operand)) {
      found = true;
      break;
    }
  }
  if (found) {
    records[i].end = step;
    printf("Found ");
    dumpOperand(records[i].operand);
    printf("\n");
  } else {
    printf("Adding ");
    dumpOperand(operand);
    printf("\n");
    arrput(records, ((struct ACCESS_RECORD){
      .operand = operand,
      .local = true, // TODO false
      .start = step,
      .end = step
    }));
  }
  return records;
}

static struct ACCESS_RECORD* scanBlock(TAC_BLOCK* block) {
  TAC* instruction = block->start;
  uint64_t step = 0;
  struct ACCESS_RECORD* records = NULL;
  while (instruction != NULL) {
    switch (instruction->tag) {
      case TAC_TYPE_COPY:
        {
          // find operand in records
          // if not exist, figure out of global, or create with start index,
          // else, record end index
          //
          records = updateRecords(records, step, instruction->op1);
          records = updateRecords(records, step, instruction->op2);
          if (instruction->op != TAC_OP_NONE
            && instruction->op != TAC_OP_NEG
            && instruction->op != TAC_OP_BITWISE_NOT
            && instruction->op != TAC_OP_NOT) {
            records = updateRecords(records, step, instruction->op3);
          }
        }
        break;
      case TAC_TYPE_IF_TRUE:
      case TAC_TYPE_IF_FALSE:
        {
          // find operand in records
          // if not exist, figure out of global, or create with start index,
          // else, record end index
          //
          records = updateRecords(records, step, instruction->op1);
        }
        break;
    }
    instruction = instruction->next;
    step++;
  }

  for (int i = 0; i < arrlen(records); i++) {
    dumpOperand(records[i].operand);
    printf(": From %i to %i\n", records[i].start, records[i].end);
  }
  return NULL;
}

static void emitBlock(FILE* f, TAC_BLOCK* block) {
  struct ACCESS_RECORD* records = scanBlock(block);
  TAC* instruction = block->start;
  while (instruction != NULL) {
    switch (instruction->tag) {
      case TAC_TYPE_COPY:
        {
          fprintf(f, "LD %s, %s\n", symbol(instruction->op1), symbol(instruction->op2));
        }
        break;
    }
    instruction = instruction->next;
  }

  arrfree(records);
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
