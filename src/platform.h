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
#include "value.h"

typedef struct {
  const char* key;
  void (*init)();
  void (*complete)();
  void (*freeRegister)(int r);
  void (*freeAllRegisters)(void);
  void (*genPreamble)(FILE* f);
  void (*genFunction)(FILE* f, STRING* name);
  void (*genFunctionEpilogue)(FILE* f, STRING* name);
  void (*genReturn)(FILE* f, STRING*, int);
  int (*genLoadRegister)(FILE* f, int, int);
  int (*genLoad)(FILE* f, int);
  int (*genIdentifier)(FILE* f, SYMBOL_TABLE_ENTRY symbol);
  int (*genIdentifierAddr)(FILE* f, SYMBOL_TABLE_ENTRY symbol);
  int (*genAssign)(FILE* f, int, int);
  int (*genAdd)(FILE* f, int, int);
  int (*genSub)(FILE* f, int, int);
  int (*genMul)(FILE* f, int, int);
  int (*genDiv)(FILE* f, int, int);
  int (*genMod)(FILE* f, int, int);
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
  void (*genSimpleExit)(FILE* f);
  void (*genExit)(FILE* f, int);
  void (*genRaw)(FILE* f, const char*);
  int (*labelCreate)();
  void (*genCmp)(FILE* f, int, int);
  void (*genJump)(FILE* f, int);
  void (*genLabel)(FILE* f, int);
} PLATFORM;

void PLATFORM_init();
void PLATFORM_shutdown();
PLATFORM PLATFORM_get(const char* name);

#endif
