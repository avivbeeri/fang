/*
  MIT License

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

#ifndef platform_h
#define platform_h

#include <stdio.h>
#include "memory.h"
#include "symbol_table.h"
#include "type_table.h"
#include "value.h"

typedef struct PLATFORM {
  const char* key;
  void (*init)();
  void (*complete)();
  void (*freeRegister)(int r);
  int (*holdRegister)(int r);
  void (*freeAllRegisters)(void);
  int (*getSize)(TYPE_ID);
  bool (*calculateSizes)();

  void (*genPreamble)(FILE* f);
  void (*genCompletePreamble)(FILE* f);
  void (*genFunction)(FILE* f, STR name, SYMBOL_TABLE_SCOPE scope);
  void (*genFunctionEpilogue)(FILE* f, STR name, SYMBOL_TABLE_SCOPE scope);
  void (*genReturn)(FILE* f, STR, int);
  int (*genLoadRegister)(FILE* f, int, int);
  int (*genLoad)(FILE* f, int, int);
  int (*genConstant)(FILE* f, int);
  int (*genIdentifierAddr)(FILE* f, SYMBOL_TABLE_ENTRY symbol);
  int (*genIdentifier)(FILE* f, SYMBOL_TABLE_ENTRY symbol);
  int (*genAssign)(FILE* f, int, int, int);
  int (*genCopyObject)(FILE* f, int, int, int);
  int (*genAdd)(FILE* f, int, int, int);
  int (*genSub)(FILE* f, int, int, int);
  int (*genMul)(FILE* f, int, int, int);
  int (*genDiv)(FILE* f, int, int, int);
  int (*genMod)(FILE* f, int, int);
  int (*genBitwiseAnd)(FILE* f, int, int);
  int (*genBitwiseOr)(FILE* f, int, int);
  int (*genBitwiseXor)(FILE* f, int, int);
  int (*genBitwiseNot)(FILE* f, int);
  int (*genShiftLeft)(FILE* f, int, int);
  int (*genShiftRight)(FILE* f, int, int);
  int (*genLessThan)(FILE* f, int, int);
  int (*genGreaterThan)(FILE* f, int, int);
  int (*genEqualLessThan)(FILE* f, int, int);
  int (*genEqualGreaterThan)(FILE* f, int, int);
  int (*genNeg)(FILE* f, int);
  int (*genLogicalNot)(FILE* f, int);
  int (*genAllocStack)(FILE* f, int, int);
  int (*genFunctionCall)(FILE* f, int, int*);
  int (*genInitSymbol)(FILE* f, SYMBOL_TABLE_ENTRY, int);
  void (*genRunMain)(FILE* f);
  void (*genSimpleExit)(FILE* f);
  void (*genExit)(FILE* f, int);
  void (*genRaw)(FILE* f, const char*);
  int (*labelCreate)();
  int (*genCmp)(FILE* f, int, int);
  void (*genEqual)(FILE* f, int, int);
  void (*genNotEqual)(FILE* f, int, int);
  void (*genJump)(FILE* f, int);
  void (*genLabel)(FILE* f, int);
  int (*genRef)(FILE* f, int);
  int (*genDeref)(FILE* f, int, int);
  int (*genIndexAddr)(FILE* f, int, int, int);
  int (*genIndexRead)(FILE* f, int, int, int);
  int (*genFieldOffset)(FILE* f, int leftReg, int typeIndex, STR fieldName);
  void (*genGlobalConstant)(FILE* f, SYMBOL_TABLE_ENTRY entry, Value value, Value count);
  void (*genGlobalVariable)(FILE* f, SYMBOL_TABLE_ENTRY entry, Value value, Value count);
  void (*reportTypeTable)(void);
  void (*beginSection)(FILE* f, STR name, STR annotation);
  void (*endSection)(FILE* f);
} PLATFORM;

void PLATFORM_init();
void PLATFORM_shutdown();
PLATFORM PLATFORM_get(const char* name);

#endif
