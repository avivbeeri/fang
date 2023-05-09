#include "common.h"
#include "platform.h"
#include "type_table.h"
#include "symbol_table.h"
#include "const_table.h"

static int labelId = 0;
static bool freereg[4];
static char *regList[4] = { "X8", "X9", "X10", "X11" };
static char *paramRegList[8] = { "X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7" };
static int MAX_PARAM_REG = 8;

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
  printf("\n");
  printf("Out of registers, abort\n");
  exit(1);
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

static int genLoad(FILE* f, int i) {
  // Load i into a register
  // return the register index
  int r = allocateRegister();
  fprintf(f, "  MOV %s, #%i\n", regList[r], i);
  return r;
}
static void genCmp(FILE* f, int r, int jumpLabel) {
  fprintf(f, "  CMP %s, #0\n", regList[r]);
  fprintf(f, "  BEQ %s\n", labelPrint(jumpLabel));
  freeRegister(r);
}
static void genCmpNotEqual(FILE* f, int r, int jumpLabel) {
  fprintf(f, "  CMP %s, #0\n", regList[r]);
  fprintf(f, "  BNE %s\n", labelPrint(jumpLabel));
  freeRegister(r);
}

static int genAllocStack(FILE* f, int storage) {
  char* store = regList[storage];
  fprintf(f, "  ADD %s, %s, #15 ; storage\n", store, store);
  fprintf(f, "  LSR %s, %s, #4\n", store, store);
  fprintf(f, "  LSL %s, %s, #4\n", store, store);
  fprintf(f, "  SUB SP, SP, %s\n", store);
  fprintf(f, "  MOV %s, SP\n", store);
  return storage;
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
/*
static int getStackOffset(SYMBOL_TABLE_ENTRY entry) {
  uint32_t offset = entry.offset;
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(entry.scopeIndex);
  uint32_t index = entry.scopeIndex;

  SYMBOL_TABLE_SCOPE current = scope;
  while (current.scopeType != SCOPE_TYPE_FUNCTION) {
    index = current.parent;
    current = SYMBOL_TABLE_getScope(index);
    offset += current.localOffset;
  }
  return offset;
}
*/

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
    snprintf(buffer, sizeof(buffer), "[FP, #%i]", (entry.paramOrdinal + 1) * 16);
    return buffer;
  } else {
    // local
    // calculate offset
    uint32_t offset = getStackOrdinal(entry);
    snprintf(buffer, sizeof(buffer), "[FP, #%i]", -offset * 16);
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

static int genLoadRegister(FILE* f, int i, int r) {
  // Load i into a register
  // return the register index
  r = r == -1 ? allocateRegister() : r;
  fprintf(f, "  MOV %s, #%i\n", regList[r], i);
  return r;
}


static int genIdentifierAddr(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  if (entry.entryType == SYMBOL_TYPE_PARAMETER) {
    fprintf(f, "  ADD %s, FP, #%i\n", regList[r], (entry.paramOrdinal + 1) * 16);
  } else if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
    fprintf(f, "  ADR %s, %s\n", regList[r], symbol(entry));
  } else {
    uint32_t offset = getStackOrdinal(entry);
    fprintf(f, "  ADD %s, FP, #%i\n", regList[r], -offset * 16);
  }
  return r;
}
static int genIdentifier(FILE* f, SYMBOL_TABLE_ENTRY entry) {
  int r = allocateRegister();
  if (entry.entryType == SYMBOL_TYPE_FUNCTION) {
    fprintf(f, "  ADR %s, %s\n", regList[r], symbol(entry));
  } else if (typeTable[entry.typeIndex].parent == U16_INDEX) {
    fprintf(f, "  LDR %s, %s\n", regList[r], symbol(entry));
  } else {
    fprintf(f, "  LDR %s, %s\n", regList[r], symbol(entry));
  }
  return r;
}

static int genRef(FILE* f, int leftReg) {
  return leftReg;
}
static int genDeref(FILE* f, int leftReg) {
  fprintf(f, "  LDR %s, [%s]\n", regList[leftReg], regList[leftReg]);
  return leftReg;
}

static int genIndexAddr(FILE* f, int leftReg, int index, int dataSize) {
  if (dataSize > 1) {
    int temp = genLoad(f, dataSize);
    fprintf(f, "  MUL %s, %s, %s\n", regList[index], regList[index], regList[temp]);
    freeRegister(temp);
  }
  fprintf(f, "  ADD %s, %s, %s; index address\n", regList[leftReg], regList[leftReg], regList[index]);
  freeRegister(index);
  return leftReg;
}

static int genIndexRead(FILE* f, int leftReg, int index, int dataSize) {
  if (dataSize > 1) {
    int temp = genLoad(f, dataSize);
    fprintf(f, "  MUL %s, %s, %s\n", regList[index], regList[index], regList[temp]);
    freeRegister(temp);
  }
  fprintf(f, "  LDR %s, [%s, %s] ; index read\n", regList[leftReg], regList[leftReg], regList[index]);
  freeRegister(index);
  return leftReg;
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
      fprintf(f, ".align %lu\n", 4 - bytes % 4);
    }
    fprintf(f, "const_%i: ", i);
    fprintf(f, ".byte %i\n", (uint8_t)(AS_STRING(constTable[i].value)->length) % 256);
    fprintf(f, ".asciz \"%s\"\n", AS_STRING(constTable[i].value)->chars);
    bytes += strlen(AS_STRING(constTable[i].value)->chars);
  }
  fprintf(f, ".global _start\n");
  fprintf(f, ".align 2\n");
  fprintf(f, "_start:\n");
  fprintf(f, "  MOV X28, #0\n");
  fprintf(f, "  MOV X0, #0\n");
  fprintf(f, "  BL _fang_main\n");
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

static void genFunction(FILE* f, STRING* name) {
  // get max function scope offset
  // and round to next 16
  // TODO: Allocate based on function local scopes
  int p = 3 * 16;

  fprintf(f, "\n_fang_%s:\n", name->chars);
  fprintf(f, "  PUSH2 LR, FP\n"); // push LR onto stack
  fprintf(f, "  MOV FP, SP\n"); // create stack frame
  fprintf(f, "  SUB SP, SP, #%i\n", p); // stack is 16 byte aligned
}

static void genFunctionEpilogue(FILE* f, STRING* name) {
  // get max function scope offset
  // and round to next 16
  fprintf(f, "\n_fang_ep_%s:\n", name->chars);

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
  fprintf(f, "  B _fang_ep_%s\n", name->chars);
}

static void genRaw(FILE* f, const char* str) {
  fprintf(f, "  %s\n", str);
}
static int genInitSymbol(FILE* f, SYMBOL_TABLE_ENTRY entry, int rvalue) {
  if (typeTable[entry.typeIndex].entryType == ENTRY_TYPE_ARRAY) {
    // allocate stack memory?
  }
  fprintf(f, "  STR %s, %s\n", regList[rvalue], symbol(entry));
  return rvalue;
}
static int genAssign(FILE* f, int lvalue, int rvalue) {
  fprintf(f, "  STR %s, [%s] ; assign\n", regList[rvalue], regList[lvalue]);
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
static int genAdd(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  ADD %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genSub(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  SUB %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genMul(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  MUL %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}
static int genDiv(FILE* f, int leftReg, int rightReg) {
  fprintf(f, "  SDIV %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int genFunctionCall(FILE* f, int callable, int* params) {
  // We need to push registers which aren't free
  // non-params first
  int* snapshot = NULL;
  for (int i = 0; i < 4; i++) {
    if (!freereg[i] && i != callable) {
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
    // fprintf(f, "  STR %s, [SP, %li]\n", regList[params[i]], (arrlen(params) - i - 1));
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

static int genGreaterThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, gt\n", regList[left]);
  fprintf(f, "  AND %s, %s, 255\n", regList[left], regList[left]);
  return left;
}
static int genEqualGreaterThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, ge\n", regList[left]);
  fprintf(f, "  AND %s, %s, 255\n", regList[left], regList[left]);
  return left;
}
static int genEqualLessThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  freeRegister(right);
  fprintf(f, "  CSET %s, le\n", regList[left]);
  fprintf(f, "  AND %s, %s, 255\n", regList[left], regList[left]);
  return left;
}

static int genLessThan(FILE* f, int left, int right) {
  fprintf(f, "  CMP %s, %s\n", regList[left], regList[right]);
  fprintf(f, "  CSET %s, lt\n", regList[left]);
  fprintf(f, "  AND %s, %s, 255\n", regList[left], regList[left]);
  return left;
}
static int genLogicalNot(FILE* f, int valueReg) {
  fprintf(f, "  CMP %s, #0\n", regList[valueReg]);
  fprintf(f, "  CSET %s, eq\n", regList[valueReg]);
  fprintf(f, "  AND %s, %s, 255\n", regList[valueReg], regList[valueReg]);
  return valueReg;
}


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
  .genLoadRegister = genLoadRegister,
  .genInitSymbol = genInitSymbol,
  .genIdentifier = genIdentifier,
  .genIdentifierAddr = genIdentifierAddr,
  .genRaw = genRaw,
  .genAssign = genAssign,
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
  .genCmpNotEqual = genCmpNotEqual,
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
  .genIndexRead = genIndexRead,
  .genIndexAddr = genIndexAddr

};

#undef emitf
