#include "common.h"
#include "platform.h"
#include "type_table.h"
#include "symbol_table.h"
#include "const_table.h"

#define emitf(fmt, ...) do { fprintf(f, fmt, ##__VA_ARGS__); } while(0)

static int labelId = 0;
static bool freereg[4];
static char *regList[4] = { "X1", "X2", "X3", "X4" };

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

static const char* symbol(SYMBOL_TABLE_ENTRY entry) {
  static char buffer[128];
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  if (entry.entryType == SYMBOL_TYPE_CONSTANT && scope.parent == 0) {
    snprintf(buffer, sizeof(buffer), "%s", entry.key);
    return buffer;
  } else {
    // local
    // calculate offset
    uint32_t offset = 0;
    for (int i = 0; i < shlen(scope.table); i++) {
      SYMBOL_TABLE_ENTRY other = scope.table[i];
      if (other.defined && other.ordinal < entry.ordinal) {
        offset += typeTable[other.typeIndex].byteSize;
      }
    }
    snprintf(buffer, sizeof(buffer), "[FP, #%i]", offset);
    return buffer;
  }
}

/*
static int labelCreate() {
  return labelId++;
}
*/


/*
const char* labelPrint(int i) {
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "L%i", i);
  return buffer;
}
*/

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
  emitf("MOV %s, #%i\n", regList[r], i);
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

static int genIdentifier(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  if (isNumeric(entry.typeIndex)) {
    emitf("LDR %s, %s\n", regList[r], symbol(entry));
  } else {
    printf("%s\n", entry.key);
    emitf("LDR %s, %s\n", regList[r], symbol(entry));
  }
  return r;
}

static void genPreamble(FILE* f) {
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
  emitf("bl _fang_main\n");
}
static void genSimpleExit(FILE* f) {
  // Returns 0;
  emitf("mov X0, #0\n");
  emitf("mov X16, #1\n");
  emitf("svc 0\n");
}

static void genExit(FILE* f, int r) {
  // Assumes return code is in reg r.
  emitf("mov X0, %s\n", regList[r]);
  emitf("mov X16, #1\n");
  emitf("svc 0\n");
}

static void genFunction(FILE* f, STRING* name) {
  emitf("_fang_%s:\n", name->chars);
  emitf("STP LR, FP, [SP, #-16]!\n"); // push LR onto stack
  emitf("SUB FP, SP, #16\n"); // create stack frame
  emitf("SUB SP, SP, #16\n"); // stack is 16 byte aligned
                              // This needs to account for all function variables
}

static void genFunctionEpilogue(FILE* f, STRING* name) {
  emitf("_fang_ep_%s:\n", name->chars);
  emitf("ADD SP, SP, #16\n");
  emitf("LDP LR, FP, [SP], #16\n"); // pop LR from stack
  emitf("RET\n");
}

static void genReturn(FILE* f, STRING* name, int r) {
  if (r != -1) {
    emitf("MOV X0, %s\n", regList[r]);
  }
  emitf("B _fang_ep_%s\n", name->chars);
}

static void genRaw(FILE* f, const char* str) {
  emitf("%s\n", str);
}
static int genAssign(FILE* f, int lvalue, int rvalue) {
  emitf("STR %s, [%s]\n", regList[rvalue], regList[lvalue]);
  freeRegister(lvalue);
  return rvalue;
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
static int genAdd(int leftReg, int rightReg) {
  emitf("add %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
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
  .genPreamble = genPreamble,
  .genExit = genExit,
  .genSimpleExit = genSimpleExit,
  .genFunction = genFunction,
  .genFunctionEpilogue = genFunctionEpilogue,
  .genReturn = genReturn,
  .genLoad = genLoad,
  .genIdentifier = genIdentifier,
  .genRaw = genRaw,
  .genAssign = genAssign
};

#undef emitf