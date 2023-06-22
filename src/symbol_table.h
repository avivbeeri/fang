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
#include "type_table.h"

struct PLATFORM;

typedef enum SYMBOL_TABLE_ENTRY_STATUS {
  SYMBOL_TABLE_STATUS_INVALID,
  SYMBOL_TABLE_STATUS_DECLARED,
  SYMBOL_TABLE_STATUS_DEFINED,
} SYMBOL_TABLE_ENTRY_STATUS;

typedef enum SYMBOL_TABLE_STORAGE_TYPE {
  STORAGE_TYPE_NONE,
  STORAGE_TYPE_LOCAL,
  STORAGE_TYPE_GLOBAL,
  STORAGE_TYPE_GLOBAL_OBJECT,
  STORAGE_TYPE_LOCAL_OBJECT,
  STORAGE_TYPE_PARAMETER
} SYMBOL_TABLE_STORAGE_TYPE;

typedef enum SYMBOL_TYPE {
  SYMBOL_TYPE_UNKNOWN,
  SYMBOL_TYPE_SHADOW,
  SYMBOL_TYPE_FUNCTION,
  SYMBOL_TYPE_PARAMETER,
  SYMBOL_TYPE_VARIABLE,
  SYMBOL_TYPE_CONSTANT,
} SYMBOL_TYPE;

typedef enum {
  SCOPE_TYPE_INVALID,
  SCOPE_TYPE_MODULE,
  SCOPE_TYPE_BANK,
  SCOPE_TYPE_FUNCTION,
  SCOPE_TYPE_BLOCK,
  SCOPE_TYPE_LOOP,
} SYMBOL_TABLE_SCOPE_TYPE;

typedef struct SYMBOL_TABLE_ENTRY {
  STR key;
  SYMBOL_TYPE entryType;
  bool defined;
  SYMBOL_TABLE_ENTRY_STATUS status;
  SYMBOL_TABLE_STORAGE_TYPE storageType;

  uint32_t ordinal; // posiiton in scope
  uint32_t paramOrdinal; // posiiton in scope
  uint32_t offset;
  TYPE_ID typeIndex;
  uint32_t scopeIndex;
  uint32_t bankIndex;
  // only for constants
  uint32_t constantIndex;
  // only for arrays
  uint32_t elementCount;
} SYMBOL_TABLE_ENTRY;


typedef struct {
  uint32_t key;
  uint32_t parent;
  STR moduleName;
  SYMBOL_TABLE_SCOPE_TYPE scopeType;
  SYMBOL_TABLE_ENTRY* table;
  uint32_t bankIndex;
  uint32_t ordinal;
  uint32_t paramOrdinal;

  uint32_t nestedCount;
  uint32_t tableAllocationCount;

  uint32_t nestedSize;
  uint32_t tableSize;
  uint32_t tableAllocationSize;
  bool leaf;
} SYMBOL_TABLE_SCOPE;

// 0 - LANGUAGE
// 1 - User-defined globals
// 2 - Function/record scopes

void SYMBOL_TABLE_init(void);
void SYMBOL_TABLE_openScope(SYMBOL_TABLE_SCOPE_TYPE scopeType);
void SYMBOL_TABLE_closeScope();
void SYMBOL_TABLE_calculateAllocations(struct PLATFORM platform);
void SYMBOL_TABLE_report();
void SYMBOL_TABLE_declare(STR name, SYMBOL_TYPE type, TYPE_ID typeIndex, SYMBOL_TABLE_STORAGE_TYPE storageType);
void SYMBOL_TABLE_define(STR name, SYMBOL_TYPE type, TYPE_ID typeIndex, SYMBOL_TABLE_STORAGE_TYPE storageType);
bool SYMBOL_TABLE_scopeHas(STR name);
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getCurrentScope();
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScopeByName(STR name);
int SYMBOL_TABLE_getScopeIndexByName(STR name);
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScope(uint32_t scopeIndex);
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrent(STR name);
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrentOnly(STR name);
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_checkBanks(STR name);
uint32_t SYMBOL_TABLE_getCurrentScopeIndex();
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_get(uint32_t scope, STR name);
bool SYMBOL_TABLE_nameScope(STR name);
void SYMBOL_TABLE_free(void);
void SYMBOL_TABLE_updateElementCount(STR name, uint32_t elementCount);
void SYMBOL_TABLE_pushScope(int index);
void SYMBOL_TABLE_popScope();
STR SYMBOL_TABLE_getNameFromStart(int start);
STR SYMBOL_TABLE_getNameFromCurrent(void);

#endif
