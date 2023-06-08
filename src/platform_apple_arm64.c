#include "common.h"
#include "platform.h"
#include "type_table.h"
#include "symbol_table.h"
#include "const_table.h"

static int labelId = 0;

#define REG_SIZE 4
static int freereg[REG_SIZE];
static char *storeRegList[REG_SIZE] = { "W8", "W9", "W10", "W11" };
static char *regList[REG_SIZE] = { "X8", "X9", "X10", "X11" };

#define BOOL_INDEX 2
#define U8_INDEX 3
#define I8_INDEX 4
#define U16_INDEX 5
#define I16_INDEX 6
#define NUMERICAL_INDEX 7
#define STRING_INDEX 8
#define FN_INDEX 9
#define CHAR_INDEX 10

static bool isPointer(int type) {
  return typeTable[type].entryType == ENTRY_TYPE_POINTER || typeTable[type].entryType == ENTRY_TYPE_ARRAY;
}

static int labelCreate() {
  return labelId++;
}

const char* labelPrint(int i) {
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "L%i", i);
  return buffer;
}

static void freeAllRegisters() {
  for (int i = 0; i < REG_SIZE; i++) {
    freereg[i] = 0;
  }
}

static void freeRegister(int r) {
  if (freereg[r] <= 0) {
    printf("Double-freeing register, abort to check");
    exit(1);
  }
  freereg[r] -= 1;
}

static int allocateRegister() {
  for (int i = 0; i < REG_SIZE; i++) {
    if (freereg[i] == 0) {
      freereg[i] += 1;
      return i;
    }
  }
  printf("\n");
  printf("Out of registers, abort\n");
  exit(1);
}

static int holdRegister(int r) {
  freereg[r]++;
  return r;
}

static bool calculateSizes() {
  // Init primitives
  /*
  TYPE_TABLE_setPrimitiveSize("void", 0);

  TYPE_TABLE_setPrimitiveSize("bool", 8);
  TYPE_TABLE_setPrimitiveSize("i8", 8);
  TYPE_TABLE_setPrimitiveSize("u8", 8);
  TYPE_TABLE_setPrimitiveSize("char", 8);

  TYPE_TABLE_setPrimitiveSize("u16", 8);
  TYPE_TABLE_setPrimitiveSize("i16", 8);

  TYPE_TABLE_setPrimitiveSize("number", 8);

  TYPE_TABLE_setPrimitiveSize("string", 8);
  TYPE_TABLE_setPrimitiveSize("fn", 8);
  TYPE_TABLE_setPrimitiveSize("ptr", 8);
  */
  TYPE_TABLE_setPrimitiveSize("void", 0);

  TYPE_TABLE_setPrimitiveSize("bool", 1);
  TYPE_TABLE_setPrimitiveSize("i8", 1);
  TYPE_TABLE_setPrimitiveSize("u8", 1);
  TYPE_TABLE_setPrimitiveSize("char", 1);

  TYPE_TABLE_setPrimitiveSize("u16", 2);
  TYPE_TABLE_setPrimitiveSize("i16", 2);

  TYPE_TABLE_setPrimitiveSize("number", 4);

  TYPE_TABLE_setPrimitiveSize("string", 8);
  TYPE_TABLE_setPrimitiveSize("fn", 8);
  TYPE_TABLE_setPrimitiveSize("ptr", 8);
  return TYPE_TABLE_calculateSizes();
}

static void genMacros(FILE* f) {
  fprintf(f, " .macro PUSH1 register\n");
  fprintf(f, "        STR \\register, [SP, #-16]!\n");
  fprintf(f, " .endm\n");
  fprintf(f, " .macro POP1 register\n");
  fprintf(f, "        LDR \\register, [SP], #16\n");
  fprintf(f, " .endm\n");
  fprintf(f, " .macro PUSH2 register1, register2\n");
  fprintf(f, "        STP \\register1, \\register2, [SP, #-16]!\n");
  fprintf(f, " .endm\n");
  fprintf(f, " .macro POP2 register1, register2\n");
  fprintf(f, "        LDP \\register1, \\register2, [SP], #16\n");
  fprintf(f, " .endm\n");
}

static void genLabel(FILE* f, int label) {
  fprintf(f, "%s:\n", labelPrint(label));
}
static void genJump(FILE* f, int label) {
  fprintf(f, "  B %s\n", labelPrint(label));
}

static int genConstant(FILE* f, int i) {
  // Load i into a register
  // return the register index
  int r = allocateRegister();
  fprintf(f, "  ADRP %s, _fang_str_%i@PAGE\n", regList[r], i);
  // Strings store their length at the front, so nudge the pointer by 1
  fprintf(f, "  ADD %s, %s, _fang_str_%i@PAGEOFF + %zu\n", regList[r], regList[r], i, typeTable[U8_INDEX].byteSize);
  return r;
}
static int genLoad(FILE* f, int i, int type) {
  // Load i into a register
  // return the register index
  int size = typeTable[type].byteSize;
  int r = allocateRegister();
  if (typeTable[type].byteSize == 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    int8_t value = i;
    fprintf(f, "  MOV %s, #%" PRIi8 "\n", regList[r], value);
  } else if (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX) {
    fprintf(f, "  MOV %s, #%i\n", regList[r], i);
    fprintf(f, "  LSL %s, %s, #56\n", regList[r], regList[r]);
    fprintf(f, "  ASR %s, %s, #56\n", regList[r], regList[r]);
  } else {
    fprintf(f, "  MOV %s, #%i\n", regList[r], i);
  }
  return r;
}
static void genEqual(FILE* f, int r, int jumpLabel) {
  fprintf(f, "  TBZ %s, #0, %s\n", regList[r], labelPrint(jumpLabel));
  freeRegister(r);
}
static void genNotEqual(FILE* f, int r, int jumpLabel) {
  fprintf(f, "  TBNZ %s, #0, %s\n", regList[r], labelPrint(jumpLabel));
  freeRegister(r);
}

static int genAllocStack(FILE* f, int storage, int type) {
  char* store = regList[storage];
  int offset = typeTable[type].byteSize;

  if (offset > 1) {
    int temp = genLoad(f, offset, 8);
    // TODO: Convert to MADD
    fprintf(f, "  MUL %s, %s, %s\n", store, store, regList[temp]);
    freeRegister(temp);
  }
  fprintf(f, "  ADD %s, %s, #15 ; storage\n", store, store);
  // ARM64 stack has to align to 16 bytes
  // We add 15, shift right 4 and shift left 4 to round to the next
  // 16.
  fprintf(f, "  LSR %s, %s, #4\n", store, store);
  fprintf(f, "  LSL %s, %s, #4\n", store, store);
  fprintf(f, "  SUB SP, SP, %s\n", store);
  fprintf(f, "  MOV %s, SP\n", store);
  return storage;
}

static int getStackOffset(SYMBOL_TABLE_ENTRY entry) {
  uint32_t offset = 0;
  uint32_t index = entry.scopeIndex;
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(index);
  SYMBOL_TABLE_SCOPE current = scope;
  for (int i = 0; i < hmlen(scope.table); i++) {
    SYMBOL_TABLE_ENTRY tableEntry = scope.table[i];
    if (tableEntry.defined) {
      if (tableEntry.elementCount > 0) {
        offset += typeTable[tableEntry.typeIndex].byteSize * tableEntry.elementCount;
      } else {
        offset += typeTable[tableEntry.typeIndex].byteSize;
      }
      // The stack is subtractive, so we need to account for the current byte size
      // when calculating its offset.

      if (tableEntry.ordinal == entry.ordinal) {
        break;
      }
    }
  }

  while (current.scopeType != SCOPE_TYPE_FUNCTION) {
    index = current.parent;
    current = SYMBOL_TABLE_getScope(index);
    for (int i = 0; i < hmlen(current.table); i++) {
      SYMBOL_TABLE_ENTRY tableEntry = current.table[i];
      if (tableEntry.elementCount > 0) {
        offset += typeTable[tableEntry.typeIndex].byteSize * tableEntry.elementCount;
      } else {
        offset += typeTable[tableEntry.typeIndex].byteSize;
      }
    }
  }
  return offset + 16; // offset by 1 from frame pointer
}
static int getStackOrdinal(SYMBOL_TABLE_ENTRY entry) {
  uint32_t ordinal = entry.ordinal;
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  uint32_t index = entry.scopeIndex;

  SYMBOL_TABLE_SCOPE current = scope;
  while (current.scopeType != SCOPE_TYPE_FUNCTION) {
    index = current.parent;
    current = SYMBOL_TABLE_getScope(index);
    ordinal += current.ordinal;
  }
  return ordinal + 1; // offset by 1 from frame pointer
}

static const char* symbol(SYMBOL_TABLE_ENTRY entry) {
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "_fang");
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  if (scope.moduleName != NULL) {
    sprintf(buffer + strlen(buffer), "_%s", scope.moduleName->chars);
  }
  if (entry.storageType == STORAGE_TYPE_GLOBAL || entry.storageType == STORAGE_TYPE_GLOBAL_OBJECT) {
    if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
      sprintf(buffer + strlen(buffer), "_fn_%s", entry.key);
    } else if (entry.entryType == SYMBOL_TYPE_CONSTANT) {
      sprintf(buffer + strlen(buffer), "_const_%s", entry.key);
    } else if (entry.entryType == SYMBOL_TYPE_VARIABLE) {
      sprintf(buffer + strlen(buffer), "_var_%s", entry.key);
    } else {
      printf("trap %d\n", __LINE__);
      exit(1);
    }
  } else if (entry.storageType == STORAGE_TYPE_PARAMETER) {
    snprintf(buffer, sizeof(buffer), "[FP, #%i] ; %s", (entry.paramOrdinal + 1) * 16, entry.key);
  } else {
    uint32_t offset = getStackOffset(entry);
    snprintf(buffer, sizeof(buffer), "[FP, #%i] ; %s", -offset, entry.key);
  }
  return buffer;
}

void init(void) {
  freeAllRegisters();
}
void complete(void) {
  int r = allocateRegister();
  freeRegister(r);
}

static int genLoadRegister(FILE* f, int i, int r) {
  // Load i into a register
  // return the register index
  r = r == -1 ? allocateRegister() : r;
  int8_t value = i;
  fprintf(f, "  MOV %s, #%"PRIi8"\n", regList[r], value);
  if (typeTable[U8_INDEX].byteSize != 1){
    fprintf(f, "  LSL %s, %s, #56\n", regList[r], regList[r]);
    fprintf(f, "  ASR %s, %s, #56\n", regList[r], regList[r]);
  }
  return r;
}


static int genIdentifierAddr(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  if (entry.storageType == STORAGE_TYPE_PARAMETER) {
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], (entry.paramOrdinal + 1) * 16, entry.key);
  } else if (entry.storageType == STORAGE_TYPE_GLOBAL_OBJECT) {
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));
  } else if (entry.storageType == STORAGE_TYPE_GLOBAL) {
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));
  } else if (entry.storageType == STORAGE_TYPE_LOCAL) {
    uint32_t offset = getStackOffset(entry);
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], -offset, entry.key);
  } else if (entry.storageType == STORAGE_TYPE_LOCAL_OBJECT) {
    uint32_t offset = getStackOffset(entry);
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], -offset, entry.key);
  }
  return r;
}

static int genDeref(FILE* f, int baseReg, int typeIndex) {
  // TODO: handle different types here
  uint32_t size = typeTable[typeIndex].byteSize;
  freeRegister(baseReg);
  int leftReg = allocateRegister();
  if (size == 1) {
    fprintf(f, "  LDURSB %s, [%s] ; deref\n", regList[leftReg], regList[baseReg]);
  } else {
    fprintf(f, "  LDUR %s, [%s]\n ; deref\n", regList[leftReg], regList[baseReg]);
  }
  return leftReg;
}

static int genIdentifier(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));
    return r;
  } else if (entry.storageType == STORAGE_TYPE_PARAMETER) {
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], (entry.paramOrdinal + 1) * 16, entry.key);
  } else if (entry.storageType == STORAGE_TYPE_GLOBAL) {
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));
  } else if (entry.storageType == STORAGE_TYPE_LOCAL) {
    uint32_t offset = getStackOffset(entry);
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], -offset, entry.key);
  } else if (entry.storageType == STORAGE_TYPE_LOCAL_OBJECT) {
    uint32_t offset = getStackOffset(entry);
    fprintf(f, "  ADD %s, FP, #%i ; %s\n", regList[r], -offset, entry.key);
    return r;
  } else if (entry.storageType == STORAGE_TYPE_GLOBAL_OBJECT) {
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));
    return r;
  }
  return genDeref(f, r, entry.typeIndex);
}

static int genRef(FILE* f, int leftReg) {
  return leftReg;
}

static int genFieldOffset(FILE* f, int baseReg, int typeIndex, STRING* fieldName) {
  TYPE_TABLE_ENTRY entry = typeTable[typeIndex];
  int offset = 0;
  for (int i = 0; i < arrlen(entry.fields); i++) {
    if (STRING_equality(entry.fields[i].name, fieldName)) {
      break;
    }
    offset += typeTable[entry.fields[i].typeIndex].byteSize;
  }

  freeRegister(baseReg);
  int leftReg = allocateRegister();
  fprintf(f, "  ADD %s, %s, #%i; field offset address\n", regList[leftReg], regList[baseReg], offset);
  return leftReg;
}
static int genIndexAddr(FILE* f, int baseReg, int index, int type) {
  int dataSize = typeTable[type].byteSize;
  if (dataSize > 1) {
    int temp = genLoad(f, dataSize, 8);
    // TODO: Convert to MADD
    freeRegister(temp);
    fprintf(f, "  MUL %s, %s, %s\n", regList[index], regList[index], regList[temp]);
  }
  // We might still be holding onto the baseReg (for initializations especially)
  // so we attempt to free add re-allocate to get a destination reg
  freeRegister(baseReg);
  freeRegister(index);
  int leftReg = allocateRegister();
  fprintf(f, "  ADD %s, %s, %s; index address\n", regList[leftReg], regList[baseReg], regList[index]);
  return leftReg;
}
static int genIndexRead(FILE* f, int baseReg, int index, int type) {
  int dataSize = typeTable[type].byteSize;
  if (dataSize > 1) {
    int temp = genLoad(f, dataSize, 8);
    // TODO: Convert to MADD
    freeRegister(temp);
    fprintf(f, "  MUL %s, %s, %s\n", regList[index], regList[index], regList[temp]);
  }
  // We might still be holding onto the baseReg (for initializations especially)
  // so we attempt to free add re-allocate to get a destination reg
  freeRegister(baseReg);
  freeRegister(index);
  int leftReg = allocateRegister();
  fprintf(f, "  ADD %s, %s, %s; index read\n", regList[leftReg], regList[baseReg], regList[index]);
  return genDeref(f, leftReg, type);
}

static void genPreamble(FILE* f) {
  genMacros(f);
  fprintf(f, "\n\n.data\n");
}
static void genCompletePreamble(FILE* f) {
  fprintf(f, ".text\n");
  for (int i = 0; i < arrlen(constTable); i++) {
    Value v = constTable[i].value;
    if (!IS_STRING(v)) {
      continue;
    }

    fprintf(f, ".balign 8\n");
    fprintf(f, "_fang_str_%i: ", i);
    if (typeTable[CHAR_INDEX].byteSize == 1) {
      fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(constTable[i].value)->length) % 256);
    } else {
      fprintf(f, ".quad %i\n", (uint8_t)(AS_STRING(constTable[i].value)->length) % 256);
    }
    if (typeTable[CHAR_INDEX].byteSize == 1) {
      fprintf(f, ".asciz \"%s\"\n", AS_STRING(constTable[i].value)->chars);
    } else {
      for (int j = 0; j < AS_STRING(constTable[i].value)->length; j++) {
        fprintf(f, ".quad '%c'\n", (char)(AS_STRING(constTable[i].value)->chars[j]));
      }
    }
  }
}

static void emitValue(FILE* f, Value value, int typeIndex) {
  if (IS_RECORD(value)) {
    Record record = AS_RECORD(value);
    int typeIndex = record.typeIndex;
    int len = arrlen(typeTable[typeIndex].fields);
    for (int i = 0; i < len; i++) {
      TYPE_TABLE_FIELD_ENTRY field = typeTable[typeIndex].fields[i];
      bool found = false;
      int j = 0;
      for (; j < arrlen(record.names); j++) {
        if (STRING_equality(field.name, record.names[j])) {
          found = true;
          break;
        }
      }
      if (found) {
        emitValue(f, record.values[j], typeTable[typeIndex].fields[i].typeIndex);
      } else {
        fprintf(f, ".quad %zu\n", typeTable[typeTable[typeIndex].fields[i].typeIndex].byteSize);
      }
    }
  } else if (IS_ARRAY(value)) {
    Value* values = AS_ARRAY(value);
    // TODO: check for 16bit nums
    for (int i = 0; i < arrlen(values); i++) {
      emitValue(f, values[i], typeTable[typeIndex].parent);
    }
//    fprintf(f, "_fang_size_const_%s: .byte %u\n", entry.key, AS_I8(count));
  } else if (IS_PTR(value)) {
    fprintf(f, ".xword _fang_str_%zu + %zu\n", AS_PTR(value), typeTable[U8_INDEX].byteSize);
  } else if (typeTable[typeIndex].byteSize == 1) {
  //  || IS_I8(value) || IS_U8(value) || IS_CHAR(value)) {
    fprintf(f, ".byte %i\n", AS_I8(value));
  } else if (IS_EMPTY(value)) {
    fprintf(f, ".quad 0\n");
  } else {
    fprintf(f, ".quad %i\n", AS_I8(value));
  }
}
static void genGlobalConstant(FILE* f, SYMBOL_TABLE_ENTRY entry, Value value, Value count) {
  uint32_t size = typeTable[entry.typeIndex].byteSize;
  fprintf(f, ".global %s\n", symbol(entry));
  fprintf(f, ".balign 8\n");
  if (IS_STRING(value)) {
    //fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(value)->length) % 256);
    //fprintf(f, ".xword _fang_str_%zu + 1\n", AS_PTR(value));
  }
  if (IS_PTR(value)) {
    Value s = CONST_TABLE_get(AS_PTR(value));

    //fprintf(f, ".xword _fang_str_%zu + 1\n", AS_PTR(value));
   // fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(s)->length) % 256);
  }
  fprintf(f, "%s: ", symbol(entry));
  if (entry.kind == SYMBOL_KIND_RECORD) {
    emitValue(f, value, 0);
  } else if (entry.kind == SYMBOL_KIND_ARRAY) {
    if (IS_STRING(value)) {
      fprintf(f, ".asciz \"%s\"\n", AS_STRING(value)->chars);
    } else if (IS_PTR(value)) {
      Value s = CONST_TABLE_get(AS_PTR(value));
      // fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(s)->length) % 256);
      fprintf(f, ".asciz \"%s\"\n", AS_STRING(s)->chars);
    } else {
      // RECORD too
      Value* values = AS_ARRAY(value);
      // TODO: check for 16bit nums
      for (int i = 0; i < arrlen(values); i++) {
        if (IS_PTR(values[i])) {
          fprintf(f, ".xword _fang_str_%zu + %zu\n", AS_PTR(values[i]), typeTable[U8_INDEX].byteSize);
        } else if (typeTable[typeTable[entry.typeIndex].parent].byteSize == 1) {
          fprintf(f, ".byte %i\n", AS_I8(values[i]));
        } else {
          fprintf(f, ".quad %i\n", AS_U8(values[i]));
        }
      }
      fprintf(f, "_fang_size_const_%s: .byte %u\n", entry.key, AS_I8(count));
    }
  } else {
    if (IS_PTR(value)) {
      fprintf(f, ".xword _fang_str_%zu + %zu\n", AS_PTR(value), typeTable[U8_INDEX].byteSize);
    } else if (typeTable[entry.typeIndex].byteSize == 1) {
    //  || IS_I8(value) || IS_U8(value) || IS_CHAR(value)) {
    fprintf(f, ".byte %i\n", AS_I8(value));
    } else {
      fprintf(f, ".quad %i\n", AS_U8(value));
    }
  }
}


static void genGlobalVariable(FILE* f, SYMBOL_TABLE_ENTRY entry, Value value, Value count) {
  uint32_t size = typeTable[entry.typeIndex].byteSize;
  fprintf(f, ".global %s\n ", symbol(entry));
  fprintf(f, ".balign 8\n");
  if (IS_STRING(value)) {
    fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(value)->length) % 256);
   // fprintf(f, ".xword _fang_str_%zu + 1\n", AS_PTR(value));
  }
  if (IS_PTR(value)) {
    Value s = CONST_TABLE_get(AS_PTR(value));
    //fprintf(f, ".xword _fang_str_%zu + 1\n", AS_PTR(value));
    fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(s)->length) % 256);
  }
  fprintf(f, "%s: ", symbol(entry));
  if (entry.kind == SYMBOL_KIND_RECORD) {
    emitValue(f, value, 0);
  } else if (entry.kind == SYMBOL_KIND_ARRAY) {
    // RECORD too
    if (IS_EMPTY(value)) {
      if (size > 8) {
        fprintf(f, ".zero %i\n", AS_U8(count) * size);
      } else {
        fprintf(f, ".fill %i, %i, 0\n", AS_U8(count), size);
      }
    } else if (IS_STRING(value)) {
      fprintf(f, ".asciz \"%s\"\n", AS_STRING(value)->chars);
    } else if (IS_PTR(value)) {
      Value s = CONST_TABLE_get(AS_PTR(value));
      // fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(s)->length) % 256);
      fprintf(f, ".asciz \"%s\"\n", AS_STRING(s)->chars);
    } else {
      Value* values = AS_ARRAY(value);
      // TODO: check for 16bit nums
      for (int i = 0; i < arrlen(values); i++) {
        if (IS_PTR(values[i])) {
          fprintf(f, ".xword _fang_str_%zu + %zu\n", AS_PTR(values[i]), typeTable[U8_INDEX].byteSize);
        } else if (typeTable[typeTable[entry.typeIndex].parent].byteSize == 1) {
          fprintf(f, ".byte %i\n", AS_I8(values[i]));
        } else {
          fprintf(f, ".quad %i\n", AS_U8(values[i]));
        }
      }
    }
    fprintf(f, "_fang_size_const_%s: .byte %u\n", entry.key, AS_I8(count));
  } else {
    if (IS_EMPTY(value)) {
      fprintf(f, ".quad 0\n");
    } else if (IS_PTR(value)) {
      fprintf(f, ".xword _fang_str_%zu + %zu\n", AS_PTR(value), typeTable[U8_INDEX].byteSize);
    } else if (typeTable[entry.typeIndex].byteSize == 1) {
      //  || IS_I8(value) || IS_U8(value) || IS_CHAR(value)) {
      fprintf(f, ".byte %i\n", AS_I8(value));
    } else {
      fprintf(f, ".quad %i\n", AS_I8(value));
    }
  }
}

static void genRunMain(FILE* f) {
  fprintf(f, ".global _start\n");
  fprintf(f, ".align 2\n");
  fprintf(f, "_start:\n");

  fprintf(f, "  MOV X0, XZR\n");
  fprintf(f, "  BL _fang_fn_main\n");
}
static void genSimpleExit(FILE* f) {
  // Returns 0;
  // fprintf(f, "mov X0, #0\n");
  fprintf(f, "  MOV X16, #1\n");
  fprintf(f, "  SVC 0\n");
}

static void genExit(FILE* f, int r) {
  // Assumes return code is in reg r.
  fprintf(f, "  MOV X0, %s\n", regList[r]);
  fprintf(f, "  MOV X16, #1\n");
  fprintf(f, "  SVC 0\n");
}

static void genFunction(FILE* f, STRING* name, SYMBOL_TABLE_SCOPE scope) {
  // get max function scope offset
  // and round to next 16
  // TODO: Allocate based on function local scopes
  // int p = (scope.tableAllocationCount + 1) * 16;

  int p = 16 + (((scope.tableAllocationSize + 15) >> 4) << 4);

  // get scope name
  STRING* module = SYMBOL_TABLE_getNameFromStart(scope.key);
  if (module == NULL) {
    fprintf(f, "\n.global _fang_fn_%s\n", name->chars);
    fprintf(f, "\n.balign 8\n");

    fprintf(f, "\n_fang_fn_%s:\n", name->chars);
  } else {
    fprintf(f, "\n.global _fang_%s_fn_%s\n", module->chars, name->chars);
    fprintf(f, "\n.balign 8\n");
    fprintf(f, "\n_fang_%s_fn_%s:\n", module->chars, name->chars);
  }
  fprintf(f, "  PUSH2 LR, FP\n"); // push LR onto stack
  fprintf(f, "  MOV FP, SP\n"); // create stack frame
  fprintf(f, "  SUB SP, SP, #%i\n", p); // stack is 16 byte aligned
}

static void genFunctionEpilogue(FILE* f, STRING* name, SYMBOL_TABLE_SCOPE scope) {
  // get max function scope offset
  // and round to next 16
  STRING* module = SYMBOL_TABLE_getNameFromStart(scope.key);
  if (module == NULL) {
    fprintf(f, "\n_fang_fn_ep_%s:\n", name->chars);
  } else {
    fprintf(f, "\n_fang_%s_fn_ep_%s:\n", module->chars, name->chars);
  }

  fprintf(f, "  MOV SP, FP\n");
  fprintf(f, "  POP2 LR, FP\n"); // pop LR from stack
  fprintf(f, "  RET\n");
}

static void genReturn(FILE* f, STRING* name, int r) {
  if (r != -1) {
    fprintf(f, "  MOV X0, %s\n", regList[r]);
    freeRegister(r);
  } else {
    fprintf(f, "  MOV X0, XZR\n");
  }
  fprintf(f, "  B _fang_fn_ep_%s\n", name->chars);
}

static void genRaw(FILE* f, const char* str) {
  fprintf(f, "  %s\n", str);
}
static int genInitSymbol(FILE* f, SYMBOL_TABLE_ENTRY entry, int rvalue) {
  if (entry.storageType == STORAGE_TYPE_GLOBAL || entry.storageType == STORAGE_TYPE_GLOBAL_OBJECT) {
    int r = allocateRegister();
    fprintf(f, "  ADRP %s, %s@PAGE\n", regList[r], symbol(entry));
    fprintf(f, "  ADD %s, %s, %s@PAGEOFF\n", regList[r], regList[r], symbol(entry));

    if (typeTable[entry.typeIndex].byteSize == 1) {
      fprintf(f, "  STURB %s, [%s]\n", storeRegList[rvalue], regList[r]);
    } else {
      fprintf(f, "  STUR %s, [%s]\n", regList[rvalue], regList[r]);
    }
    freeRegister(r);
  } else if (entry.storageType == STORAGE_TYPE_LOCAL || entry.storageType == STORAGE_TYPE_LOCAL_OBJECT) {
    if (typeTable[entry.typeIndex].byteSize == 1) {
      fprintf(f, "  STURB %s, %s\n", storeRegList[rvalue], symbol(entry));
    } else {
      fprintf(f, "  STUR %s, %s\n", regList[rvalue], symbol(entry));
    }
  }
  return rvalue;
}
static int genCopyObject(FILE* f, int lvalue, int rvalue, int type) {
  int size = typeTable[type].byteSize;
  int current = size;
  int r = allocateRegister();
  while (current > 0) {
    if (current >= 8) {
      current -= 8;
      fprintf(f, "  LDR %s, [%s], #8\n", regList[r], regList[rvalue]);
      fprintf(f, "  STR %s, [%s], #8 ; copy\n", regList[r], regList[lvalue]);
    } else if (current >= 4) {
      current -= 4;
      fprintf(f, "  LDR %s, [%s], #4\n", storeRegList[r], regList[rvalue]);
      fprintf(f, "  STR %s, [%s], #4 ; copy\n", storeRegList[r], regList[lvalue]);
    } else if (current >= 2) {
      current -= 2;
      fprintf(f, "  LDRH %s, [%s], #2\n", storeRegList[r], regList[rvalue]);
      fprintf(f, "  STRH %s, [%s], #2 ; copy\n", storeRegList[r], regList[lvalue]);
    } else {
      current -= 1;
      fprintf(f, "  LDRB %s, [%s], #1\n", storeRegList[r], regList[rvalue]);
      fprintf(f, "  STRB %s, [%s], #1 ; copy\n", storeRegList[r], regList[lvalue]);
    }
  }
  freeRegister(r);
  freeRegister(rvalue);
  return lvalue;
}
static int genAssign(FILE* f, int lvalue, int rvalue, int type) {
  int size = typeTable[type].byteSize;
  if (size == 1) {
    fprintf(f, "  STURB %s, [%s] ; assign\n", storeRegList[rvalue], regList[lvalue]);
  } else {
    fprintf(f, "  STUR %s, [%s] ; assign\n", regList[rvalue], regList[lvalue]);
  }
  freeRegister(lvalue);
  return rvalue;
}

static int genBitwiseNot(FILE* f, int leftReg) {
  fprintf(f, "  MVN %s, %s\n", regList[leftReg], regList[leftReg]);
  return leftReg;
}
static int genBitwiseXor(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  EOR %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genBitwiseOr(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  ORR %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genBitwiseAnd(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  AND %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genAdd(FILE* f, int leftReg, int rightReg, int type) {
  int size = typeTable[type].byteSize;
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  LSL %s, %s, #56\n", regList[leftReg], regList[leftReg]);
    fprintf(f, "  LSL %s, %s, #56\n", regList[rightReg], regList[rightReg]);
  }
  fprintf(f, "  ADDS %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  ASR %s, %s, #56\n", regList[leftReg], regList[leftReg]);
  }

  freeRegister(rightReg);
  return leftReg;
}

static int genSub(FILE* f, int leftReg, int rightReg, int type) {
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  LSL %s, %s, #56\n", regList[leftReg], regList[leftReg]);
    fprintf(f, "  LSL %s, %s, #56\n", regList[rightReg], regList[rightReg]);
  }
  fprintf(f, "  SUBS %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  ASR %s, %s, #56\n", regList[leftReg], regList[leftReg]);
  }
  freeRegister(rightReg);
  return leftReg;
}

static int genMul(FILE* f, int leftReg, int rightReg, int type) {
  int size = typeTable[type].byteSize;
  fprintf(f, "  MUL %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  AND %s, %s, #255\n", regList[leftReg], regList[leftReg]);
  }
  freeRegister(rightReg);
  return leftReg;
}
static int genDiv(FILE* f, int leftReg, int rightReg, int type) {
  int size = typeTable[type].byteSize;
  fprintf(f, "  SDIV %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  if (typeTable[type].byteSize != 1 && (type == I8_INDEX || type == U8_INDEX || type == CHAR_INDEX || type == BOOL_INDEX)) {
    fprintf(f, "  AND %s, %s, #255\n", regList[leftReg], regList[leftReg]);
  }
  freeRegister(rightReg);
  return leftReg;
}

static int genFunctionCall(FILE* f, int callable, int* params) {
  // We need to push registers which aren't free
  // non-params first
  int* snapshot = NULL;
  for (int i = 0; i < 4; i++) {
    if (freereg[i] > 0 && i != callable) {
      bool found = false;
      for (int j = arrlen(params) - 1; j >= 0; j--) {
        if (params[j] == i) {
          found = true;
          break;
        }
      }
      if (!found) {
        fprintf(f, "  PUSH1 %s\n", regList[i]);
        arrput(snapshot, i);
      }
    }
  }

  for (int i = arrlen(params) - 1; i >= 0; i--) {
    fprintf(f, "  PUSH1 %s\n", regList[params[i]]);
    freeRegister(params[i]);
  }
  fprintf(f, "  BLR %s\n", regList[callable]);
  fprintf(f, "  MOV %s, X0\n", regList[callable]);
  fprintf(f, "  ADD SP, SP, #%li\n", (arrlen(params)) * 16);
  for (int i = arrlen(snapshot) - 1; i >= 0; i--) {
    fprintf(f, "  POP1 %s\n", regList[i]);
  }
  arrfree(snapshot);

  return callable;
}

static int genMod(FILE* f, int leftReg, int rightReg) {
  int r = allocateRegister();
  fprintf(f, "  UDIV %s, %s, %s\n", regList[r], regList[leftReg], regList[rightReg]);
  fprintf(f, "  MSUB %s, %s, %s, %s\n", regList[leftReg], regList[r], regList[rightReg], regList[leftReg]);
  fprintf(f, "  AND %s, %s, #255\n", regList[leftReg], regList[leftReg]);
  freeRegister(r);
  freeRegister(rightReg);
  return leftReg;
}
static int genShiftLeft(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  LSL %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genShiftRight(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  LSR %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genNeg(FILE* f, int valueReg) {
  fprintf(f, "  NEG %s, %s\n", regList[valueReg], regList[valueReg]);
  return valueReg;
}

static int genCmp(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  return left;
}
static int genGreaterThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, gt\n", regList[left]);
  fprintf(f, "  AND %s, %s, #0x1\n", regList[left], regList[left]);
  return left;
}
static int genEqualGreaterThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, ge\n", regList[left]);
  fprintf(f, "  AND %s, %s, #0x1\n", regList[left], regList[left]);
  return left;
}
static int genEqualLessThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, le\n", regList[left]);
  fprintf(f, "  AND %s, %s, #0x1\n", regList[left], regList[left]);
  return left;
}

static int genLessThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  fprintf(f, "  CSET %s, lt\n", regList[left]);
  fprintf(f, "  AND %s, %s, #0x1\n", regList[left], regList[left]);
  return left;
}
static int genLogicalNot(FILE* f, int valueReg) {
  fprintf(f, "  CMP %s, #0\n", regList[valueReg]);
  fprintf(f, "  CSET %s, eq\n", regList[valueReg]);
  fprintf(f, "  AND %s, %s, #255\n", regList[valueReg], regList[valueReg]);
  return valueReg;
}


PLATFORM platform_apple_arm64 = {
  .key = "apple_arm64",
  .init = init,
  .complete = complete,
  .calculateSizes = calculateSizes,
  .freeRegister = freeRegister,
  .holdRegister = holdRegister,
  .freeAllRegisters = freeAllRegisters,
  .genPreamble = genPreamble,
  .genCompletePreamble = genCompletePreamble,
  .genExit = genExit,
  .genRunMain = genRunMain,
  .genSimpleExit = genSimpleExit,
  .genFunction = genFunction,
  .genFunctionEpilogue = genFunctionEpilogue,
  .genReturn = genReturn,
  .genConstant = genConstant,
  .genLoad = genLoad,
  .genLoadRegister = genLoadRegister,
  .genInitSymbol = genInitSymbol,
  .genIdentifierAddr = genIdentifierAddr,
  .genIdentifier = genIdentifier,
  .genRaw = genRaw,
  .genAssign = genAssign,
  .genCopyObject = genCopyObject,
  .genAdd = genAdd,
  .genSub = genSub,
  .genMul = genMul,
  .genDiv = genDiv,
  .genMod = genMod,
  .genLogicalNot = genLogicalNot,
  .genNeg = genNeg,
  .genFunctionCall = genFunctionCall,
  .genAllocStack = genAllocStack,
  .labelCreate = labelCreate,
  .genCmp = genCmp,
  .genEqual = genEqual,
  .genNotEqual = genNotEqual,
  .genJump = genJump,
  .genLabel = genLabel,
  .genLessThan = genLessThan,
  .genGreaterThan = genGreaterThan,
  .genEqualLessThan = genEqualLessThan,
  .genEqualGreaterThan = genEqualGreaterThan,
  .genShiftLeft = genShiftLeft,
  .genShiftRight = genShiftRight,
  .genBitwiseAnd = genBitwiseAnd,
  .genBitwiseOr = genBitwiseOr,
  .genBitwiseXor = genBitwiseXor,
  .genBitwiseNot = genBitwiseNot,
  .genRef = genRef,
  .genDeref = genDeref,
  .genIndexAddr = genIndexAddr,
  .genIndexRead = genIndexRead,
  .genGlobalVariable = genGlobalVariable,
  .genGlobalConstant = genGlobalConstant,
  .genFieldOffset = genFieldOffset


};

#undef emitf
