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
  bool defined;

  uint32_t ordinal; // posiiton in scope
  uint32_t paramOrdinal; // posiiton in scope
  uint32_t offset;
  int typeIndex;
  uint32_t scopeIndex;
  // only for constants
  uint32_t constantIndex;
} SYMBOL_TABLE_ENTRY;

typedef enum {
  SCOPE_TYPE_INVALID,
  SCOPE_TYPE_MODULE,
  SCOPE_TYPE_FUNCTION,
  SCOPE_TYPE_BLOCK,
  SCOPE_TYPE_LOOP,
} SYMBOL_TABLE_SCOPE_TYPE;

typedef struct {
  uint32_t key;
  uint32_t parent;
  STRING* moduleName;
  SYMBOL_TABLE_SCOPE_TYPE scopeType;
  SYMBOL_TABLE_ENTRY* table;
  uint32_t ordinal;
  uint32_t paramOrdinal;
  uint32_t localOffset;
  uint32_t paramOffset;
  uint32_t nestedSize;
  uint32_t nestedCount;
  uint32_t tableAllocationSize;
  uint32_t tableAllocationCount;
} SYMBOL_TABLE_SCOPE;

// 0 - LANGUAGE
// 1 - User-defined globals
// 2 - Function/record scopes

void SYMBOL_TABLE_init(void);
void SYMBOL_TABLE_openScope(SYMBOL_TABLE_SCOPE_TYPE scopeType);
void SYMBOL_TABLE_closeScope();
void SYMBOL_TABLE_report();
void SYMBOL_TABLE_putFn(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex);
void SYMBOL_TABLE_put(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex);
bool SYMBOL_TABLE_scopeHas(STRING* name);
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getCurrentScope();
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScopeByName(STRING* name);
int SYMBOL_TABLE_getScopeIndexByName(STRING* name);
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScope(uint32_t scopeIndex);
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrent(STRING* name);
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrentOnly(STRING* name);
uint32_t SYMBOL_TABLE_getCurrentScopeIndex();
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_get(uint32_t scope, STRING* name);
void SYMBOL_TABLE_nameScope(STRING* name);
void SYMBOL_TABLE_free(void);

#endif
