#include "common.h"
#include "platform.h"
#include "type_table.h"
#include "symbol_table.h"
#include "const_table.h"

#define emitf(fmt, ...) do { fprintf(f, fmt, ##__VA_ARGS__); } while(0)

static int labelId = 0;
static bool freereg[4];
static char *regList[4] = { "X8", "X9", "X10", "X11" };
static char *paramRegList[8] = { "X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7" };
static int MAX_PARAM_REG = 8;

static int labelCreate() {
  return labelId++;
}

const char* labelPrint(int i) {
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "L%i", i);
  return buffer;
}

static void freeAllRegisters() {
  for (int i = 0; i < sizeof(freereg); i++) {
    freereg[i] = true;
  }
}

static void freeRegister(int r) {
  if (freereg[r]) {
    printf("Double-freeing register, abort to check");
    exit(1);
  }
  freereg[r] = true;
}

static int allocateRegister() {
  for (int i = 0; i < sizeof(freereg); i++) {
    if (freereg[i]) {
      freereg[i] = false;
      return i;
    }
  }
  printf("Out of registers, abort");
  exit(1);
}

/*
static int min(int i, int j) {
  if (i < j) {
    return i;
  }
  return j;
}
*/

static void genMacros(FILE* f) {
  emitf(" .macro PUSH1 register\n");
  emitf("        STR \\register, [SP, #-16]!\n");
  emitf(" .endm\n");
  emitf(" .macro POP1 register\n");
  emitf("        LDR \\register, [SP], #16\n");
  emitf(" .endm\n");
  emitf(" .macro PUSH2 register1, register2\n");
  emitf("        STP \\register1, \\register2, [SP, #-16]!\n");
  emitf(" .endm\n");
  emitf(" .macro POP2 register1, register2\n");
  emitf("        LDP \\register1, \\register2, [SP], #16\n");
  emitf(" .endm\n");
}

static int genAllocStack(FILE* f, int r, int storage) {
  // emitf("  SUB SP, SP, %s\n", regList[storage]);
  emitf("  ADD X28, X28, %s\n", regList[storage]);
  emitf("  ADD %s, SP, X28\n", regList[r]);

  freeRegister(storage);
  return storage;
}

static int getStackOffset(SYMBOL_TABLE_ENTRY entry) {
  uint32_t offset = 0;
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  for (int i = 0; i < shlen(scope.table); i++) {
    SYMBOL_TABLE_ENTRY other = scope.table[i];
    // TODO handle pushed params
    if (other.defined && other.ordinal < entry.ordinal/*  && other.entryType != SYMBOL_TYPE_PARAMETER */) {
      offset += typeTable[other.typeIndex].byteSize;
    }
  }
  return offset;
}

static const char* symbol(SYMBOL_TABLE_ENTRY entry) {
  static char buffer[128];
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
    snprintf(buffer, sizeof(buffer), "_fang_%s", entry.key);
    return buffer;
  } else if (entry.entryType == SYMBOL_TYPE_CONSTANT && scope.parent == 0) {
    snprintf(buffer, sizeof(buffer), "%s", entry.key);
    return buffer;
  } else if (entry.entryType == SYMBOL_TYPE_PARAMETER) {
    snprintf(buffer, sizeof(buffer), "[SP, #%i]", entry.ordinal);
    return buffer;
  } else {
    // local
    // calculate offset
    uint32_t offset = getStackOffset(entry);
    snprintf(buffer, sizeof(buffer), "[FP, #%i]", offset);
    return buffer;
  }
}

void init(void) {
  freeAllRegisters();
}
void complete(void) {
  int r = allocateRegister();
  freeRegister(r);
}

static int genLoad(FILE* f, int i) {
  // Load i into a register
  // return the register index
  int r = allocateRegister();
  emitf("  MOV %s, #%i\n", regList[r], i);
  return r;
}

#define BOOL_INDEX 2
#define U8_INDEX 3
#define I8_INDEX 4
#define U16_INDEX 5
#define I16_INDEX 6
#define NUMERICAL_INDEX 7
#define FN_INDEX 9
static bool isNumeric(int type) {
  return type <= NUMERICAL_INDEX && type >= BOOL_INDEX;
}

static int genIdentifierAddr(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  emitf("  MOV %s, FP\n", regList[r]);
  emitf("  ADD %s, %s, #%i\n", regList[r], regList[r], getStackOffset(entry));
  return r;
}
static int genIdentifier(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
    emitf("  ADR %s, %s\n", regList[r], symbol(entry));
  } else if (isNumeric(entry.typeIndex)) {
    emitf("  LDR %s, %s\n", regList[r], symbol(entry));
  } else {
    emitf("  LDR %s, %s\n", regList[r], symbol(entry));
  }
  return r;
}

static void genPreamble(FILE* f) {
  genMacros(f);
  size_t bytes = 0;
  for (int i = 0; i < arrlen(constTable); i++) {
    Value v = constTable[i].value;
    if (!IS_STRING(v)) {
      continue;
    }
    if (bytes % 4 != 0) {
      emitf(".align %lu\n", 4 - bytes % 4);
    }
    emitf("const_%i: ", i);
    emitf(".byte %i\n", (uint8_t)(AS_STRING(constTable[i].value)->length) % 256);
    emitf(".asciz \"%s\"\n", AS_STRING(constTable[i].value)->chars);
    bytes += strlen(AS_STRING(constTable[i].value)->chars);
  }
  emitf(".global _start\n");
  emitf(".align 2\n");
  emitf("_start:\n");
  emitf("  MOV X0, #0\n");
  emitf("  BL _fang_main\n");
}
static void genSimpleExit(FILE* f) {
  // Returns 0;
  // emitf("mov X0, #0\n");
  emitf("  MOV X16, #1\n");
  emitf("  SVC 0\n");
}

static void genExit(FILE* f, int r) {
  // Assumes return code is in reg r.
  emitf("  MOV X0, %s\n", regList[r]);
  emitf("  MOV X16, #1\n");
  emitf("  SVC 0\n");
}

static void genFunction(FILE* f, STRING* name) {
  emitf("\n_fang_%s:\n", name->chars);
  emitf("  PUSH2 LR, FP\n"); // push LR onto stack
  // emitf("  STR X28, [SP, #-16]!\n");
  emitf("  SUB FP, SP, #16\n"); // create stack frame
  emitf("  SUB SP, SP, #16\n"); // stack is 16 byte aligned
  // This needs to account for all function variables
}

static void genFunctionEpilogue(FILE* f, STRING* name) {
  emitf("\n_fang_ep_%s:\n", name->chars);

  // emitf("  ADD SP, SP, X28\n");
  // emitf("  LDR X28, [SP], #16\n");
  emitf("  ADD SP, SP, #16\n");
  emitf("  POP2 LR, FP\n"); // pop LR from stack
  emitf("  RET\n");
}

static void genReturn(FILE* f, STRING* name, int r) {
  if (r != -1) {
    emitf("  MOV X0, %s\n", regList[r]);
  }
  emitf("  B _fang_ep_%s\n", name->chars);
}

static void genRaw(FILE* f, const char* str) {
  emitf("  %s\n", str);
}
static int genInitSymbol(FILE* f, SYMBOL_TABLE_ENTRY entry, int rvalue) {
  if (typeTable[entry.typeIndex].entryType == ENTRY_TYPE_ARRAY) {
    // allocate stack memory
  }
  emitf("  STR %s, %s\n", regList[rvalue], symbol(entry));
  return rvalue;
}
static int genAssign(FILE* f, int lvalue, int rvalue) {
  emitf("  STR %s, [%s]\n", regList[rvalue], regList[lvalue]);
  freeRegister(lvalue);
  return rvalue;
}

static int genAdd(FILE* f, int leftReg, int rightReg) {
  emitf("  ADD %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genFunctionCall(FILE* f, int callable, int* params) {
  for (int i = arrlen(params) - 1; i >= 0; i--) {
    //emitf("MOV X0, %s\n", regList[params[i]]);
    if (i == 0) {
      emitf("  STR %s, [sp]\n", regList[params[i]]);
    } else {
      emitf("  STR %s, [sp, #%i]\n", regList[params[i]], (i - MAX_PARAM_REG) * 8);
    }
    freeRegister(params[i]);
  }
  /*
  for (int i = min(arrlen(params), MAX_PARAM_REG)  - 1; i >= 0; i--) {
    emitf("MOV %s, %s\n", paramRegList[i], regList[params[i]]);
    freeRegister(params[i]);
  }
  */
  emitf("  BLR %s\n", regList[callable]);
  emitf("  MOV %s, X0\n", regList[callable]);
  return callable;
}

/*
static void genPostamble() {
  size_t bytes = 0;
  emitf(".text\n");
  for (int i = 0; i < arrlen(constTable); i++) {
    if (bytes % 4 != 0) {
      emitf(".align %lu\n", 4 - bytes % 4);
    }
    emitf("const_%i: ", i);
    emitf(".byte %i\n", (uint8_t)(AS_STRING(constTable[i].value)->length - 1) % 256);
    emitf(".asciz \"%s\"\n", AS_STRING(constTable[i].value)->chars);
    bytes += strlen(AS_STRING(constTable[i].value)->chars);
  }
}
*/


/*
static int genMod(int leftReg, int rightReg) {
  int r = allocateRegister();
  emitf("udiv %s, %s, %s\n", regList[r], regList[leftReg], regList[rightReg]);
  emitf("msub %s, %s, %s, %s\n", regList[leftReg], regList[2], regList[rightReg], regList[leftReg]);
  freeRegister(r);
  freeRegister(rightReg);
  return leftReg;
}
static int genDiv(int leftReg, int rightReg) {
  emitf("sdiv %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genSub(int leftReg, int rightReg) {
  emitf("sub %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genMul(int leftReg, int rightReg) {
  emitf("mul %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genNegate(int valueReg) {
  emitf("neg %s, %s\n", regList[valueReg], regList[valueReg]);
  return valueReg;
}
*/


PLATFORM platform_apple_arm64 = {
  .key = "apple_arm64",
  .init = init,
  .complete = complete,
  .freeRegister = freeRegister,
  .freeAllRegisters = freeAllRegisters,
  .genPreamble = genPreamble,
  .genExit = genExit,
  .genSimpleExit = genSimpleExit,
  .genFunction = genFunction,
  .genFunctionEpilogue = genFunctionEpilogue,
  .genReturn = genReturn,
  .genLoad = genLoad,
  .genInitSymbol = genInitSymbol,
  .genIdentifier = genIdentifier,
  .genIdentifierAddr = genIdentifierAddr,
  .genRaw = genRaw,
  .genAssign = genAssign,
  .genAdd = genAdd,
  .genFunctionCall = genFunctionCall,
  .genAllocStack = genAllocStack
};

#undef emitf
