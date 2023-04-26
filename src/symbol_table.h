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

#ifndef symbol_table_h
#define symbol_table_h

#include "memory.h"
typedef enum SYMBOL_TYPE {
  SYMBOL_TYPE_UNKNOWN,
  SYMBOL_TYPE_KEYWORD,
  SYMBOL_TYPE_FUNCTION,
  SYMBOL_TYPE_PARAMETER,
  SYMBOL_TYPE_VARIABLE,
  SYMBOL_TYPE_CONSTANT,
} SYMBOL_TYPE;

typedef struct SYMBOL_TABLE_ENTRY {
  char* key;
  SYMBOL_TYPE entryType;
  int typeIndex;
  bool defined;
  uint32_t pointer;
  uint32_t array;
  uint32_t scopeIndex;
} SYMBOL_TABLE_ENTRY;

// 0 - LANGUAGE
// 1 - User-defined globals
// 2 - Function/record scopes

void SYMBOL_TABLE_init(void);
void SYMBOL_TABLE_openScope();
void SYMBOL_TABLE_closeScope();
void SYMBOL_TABLE_report();
void SYMBOL_TABLE_put(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex);
bool SYMBOL_TABLE_scopeHas(STRING* name);
void SYMBOL_TABLE_free(void);

#endif
