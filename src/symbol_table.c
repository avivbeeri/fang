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

#include "common.h"
#include "memory.h"
#include "symbol_table.h"

SYMBOL_TABLE_ENTRY* symbolTable = NULL;

SYMBOL_TABLE_ENTRY* SYMBOL_TABLE_init() {
  return symbolTable;
}

int SYMBOL_TABLE_registerType(STRING* name, size_t size, size_t parent) {
  // arrput(symbolTable, ((SYMBOL_TABLE_ENTRY){ .name = name, .byteSize = size, .parent = parent }));
  return arrlen(symbolTable);
}

void SYMBOL_TABLE_free() {
  for (int i = 0; i < arrlen(symbolTable); i++) {
    STRING_free(symbolTable[i].name);
  }
  arrfree(symbolTable);
}
