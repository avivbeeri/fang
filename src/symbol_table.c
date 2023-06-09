/*
 *
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

#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "type_table.h"
#include "platform.h"
#include "symbol_table.h"
#include <math.h>

uint32_t scopeId = 1;
int* scopeStack = NULL;
int* leafScopes = NULL;
uint32_t bankId = 1; // bank id starts at 1

SYMBOL_TABLE_SCOPE* scopes = NULL;

void SYMBOL_TABLE_openScope(SYMBOL_TABLE_SCOPE_TYPE scopeType) {
  uint32_t parent = 0;
  uint32_t bank = 0;
  if (scopeStack != NULL) {
    parent = scopeStack[arrlen(scopeStack) - 1];
  }
  if (scopeType == SCOPE_TYPE_INVALID) {
    bank = 0;
  } else if (scopeType == SCOPE_TYPE_BANK) {
    bank = bankId++;
  } else if (parent != 0) {
    SYMBOL_TABLE_SCOPE parentScope = hmgets(scopes, parent);
    bank = parentScope.bankIndex;
  }
  hmputs(scopes, ((SYMBOL_TABLE_SCOPE){
        .key = scopeId,
        .parent = parent,
        .moduleName = EMPTY_STRING,
        .scopeType = scopeType,
        .table = NULL,
        .bankIndex = bank,
        .ordinal = 0,
        .paramOrdinal = 0,
        .nestedCount = 0,
        .tableAllocationCount = 0,
        .nestedSize = 0,
        .tableSize = 0,
        .tableAllocationSize = 0,
        .leaf = true
  }));

  // Prevent memory leaks somehow
  SYMBOL_TABLE_ENTRY defaultEntry = {};
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeId);
  hmdefaults(scope.table, defaultEntry);
  hmputs(scopes, scope);

  arrput(scopeStack, scopeId);
  scopeId++;
}

void SYMBOL_TABLE_pushScope(int index) {
  arrput(scopeStack, index);
}
void SYMBOL_TABLE_popScope() {
  arrdel(scopeStack, arrlen(scopeStack) - 1);
}

bool SYMBOL_TABLE_scopeHas(STR name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    int i = hmgeti(scope.table, name);
    if (i != -1) {
      return true;
    }
    current = scope.parent;
  }
  return false;
}

static uint32_t SYMBOL_TABLE_calculateTableSize(PLATFORM p, uint32_t index) {
  SYMBOL_TABLE_SCOPE closingScope = SYMBOL_TABLE_getScope(index);
  uint32_t size = 0;
  uint32_t scopeCount = hmlen(closingScope.table);
  for (int i = 0; i < scopeCount; i++) {
    SYMBOL_TABLE_ENTRY tableEntry = closingScope.table[i];
    if (tableEntry.defined) {
      if (tableEntry.entryType == SYMBOL_TYPE_SHADOW || tableEntry.entryType == SYMBOL_TYPE_PARAMETER) {
        continue;
      }
      if (tableEntry.elementCount > 0) {
        size += p.getSize(TYPE_getParentId(tableEntry.typeIndex)) * tableEntry.elementCount;
      } else {
        size += p.getSize(tableEntry.typeIndex);
      }
    }
  }

  return size;
}
static void SYMBOL_TABLE_calculateAllocation(PLATFORM p, uint32_t start) {
  SYMBOL_TABLE_SCOPE current = SYMBOL_TABLE_getScope(start);
  uint32_t index = start;
  while (current.scopeType != SCOPE_TYPE_MODULE && current.scopeType != SCOPE_TYPE_INVALID) {
    SYMBOL_TABLE_SCOPE parent = SYMBOL_TABLE_getScope(current.parent);
    current.tableAllocationSize = current.tableSize + current.nestedSize;
    parent.nestedSize = fmax(parent.nestedSize, current.tableAllocationSize);

    hmputs(scopes, current);
    hmputs(scopes, parent);
    index = current.parent;
    current = SYMBOL_TABLE_getScope(index);
  }
}

void SYMBOL_TABLE_calculateAllocations(PLATFORM p) {
  // Cache the table sizes
  for (int i = 0; i < hmlen(scopes); i++) {
    SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(i);
    scope.tableSize = SYMBOL_TABLE_calculateTableSize(p, i);
    hmputs(scopes, scope);
  }

  for (int i = 0; i < arrlen(leafScopes); i++) {
    SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(leafScopes[i]);
    if (scope.leaf) {
      SYMBOL_TABLE_calculateAllocation(p, leafScopes[i]);
    }
  }
}

void SYMBOL_TABLE_updateElementCount(STR name, uint32_t elementCount) {
  uint32_t current = SYMBOL_TABLE_getCurrentScopeIndex();
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    SYMBOL_TABLE_ENTRY entry = hmgets(scope.table, name);
    if (entry.defined) {
      entry.elementCount = elementCount;
      hmputs(scope.table, entry);
      return;
    }
    current = scope.parent;
  }
}

void SYMBOL_TABLE_closeScope() {
  uint32_t current = SYMBOL_TABLE_getCurrentScopeIndex();
  SYMBOL_TABLE_SCOPE closingScope = SYMBOL_TABLE_getScope(current);
  SYMBOL_TABLE_SCOPE parent = SYMBOL_TABLE_getScope(closingScope.parent);

  uint32_t scopeCount = hmlen(closingScope.table);

  closingScope.tableAllocationCount = scopeCount + closingScope.nestedCount;
  parent.nestedCount = fmax(parent.nestedCount, closingScope.tableAllocationCount);
  parent.leaf = false;

  hmputs(scopes, closingScope);
  hmputs(scopes, parent);
  arrdel(scopeStack, arrlen(scopeStack) - 1);

  if (closingScope.leaf) {
    arrput(leafScopes, current);
  }
}

uint32_t SYMBOL_TABLE_getCurrentScopeIndex() {
  if (arrlen(scopeStack) == 0) {
    return 0;
  }
  return scopeStack[arrlen(scopeStack) - 1];
}

void SYMBOL_TABLE_init(void) {
  SYMBOL_TABLE_SCOPE defaultScope = {};
  hmdefaults(scopes, defaultScope);
  SYMBOL_TABLE_openScope(SCOPE_TYPE_INVALID);
}

void SYMBOL_TABLE_declare(STR name, SYMBOL_TYPE type, TYPE_ID typeIndex, SYMBOL_TABLE_STORAGE_TYPE storageType) {
  uint32_t scopeIndex = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeIndex);

  SYMBOL_TABLE_ENTRY entry = {
    .key = name,
    .defined = true,
    .entryType = type,
    .status = SYMBOL_TABLE_STATUS_DECLARED,
    .typeIndex = typeIndex,
    .scopeIndex = scopeIndex,
    .bankIndex = scope.bankIndex,
    .constantIndex = 0
  };
  hmputs(scope.table, entry);
  hmputs(scopes, scope);
}
void SYMBOL_TABLE_define(STR name, SYMBOL_TYPE type, TYPE_ID typeIndex, SYMBOL_TABLE_STORAGE_TYPE storageType) {
  uint32_t scopeIndex = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeIndex);

  uint32_t offset = 0;

  SYMBOL_TABLE_ENTRY entry = {
    .key = name,
    .entryType = type,
    .defined = true,
    .status = SYMBOL_TABLE_STATUS_DEFINED,
    .storageType = storageType,
    .typeIndex = typeIndex,
    .scopeIndex = scopeIndex,
    .bankIndex = scope.bankIndex,
    .offset = offset,
    .ordinal = scope.ordinal,
    .paramOrdinal = scope.paramOrdinal,
    .constantIndex = 0
  };
  if (type == SYMBOL_TYPE_VARIABLE || type == SYMBOL_TYPE_CONSTANT) {
    scope.ordinal++;
  } else if (type == SYMBOL_TYPE_PARAMETER) {
    scope.paramOrdinal++;
  }
  hmputs(scope.table, entry);
  hmputs(scopes, scope);
}

SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getCurrentScope() {
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, SYMBOL_TABLE_getCurrentScopeIndex());
  return scope;
}
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScope(uint32_t scopeId) {
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeId);
  return scope;
}

SYMBOL_TABLE_ENTRY SYMBOL_TABLE_get(uint32_t scopeIndex, STR name) {
  uint32_t current = scopeIndex;
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    SYMBOL_TABLE_ENTRY entry = hmgets(scope.table, name);
    TYPE_ID type = 0;
    if (entry.defined) {
      if (entry.entryType == SYMBOL_TYPE_SHADOW) {
        type = entry.typeIndex;
      } else {
        if (type != 0) {
          entry.typeIndex = type;
        }
        return entry;
      }
    }
    current = scope.parent;
  }
  return (SYMBOL_TABLE_ENTRY){0};
}
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrentOnly(STR name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
  SYMBOL_TABLE_ENTRY entry = hmgets(scope.table, name);
  if (entry.defined) {
    return entry;
  }
  return (SYMBOL_TABLE_ENTRY){0};
}
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_checkBanks(STR name) {
  for (int i = 0; i < hmlen(scopes); i++) {
    SYMBOL_TABLE_SCOPE scope = scopes[i];
    if (scope.scopeType == SCOPE_TYPE_BANK) {
      SYMBOL_TABLE_ENTRY entry = hmgets(scope.table, name);
      if (entry.defined) {
        return entry;
      }
    }
  }

  return (SYMBOL_TABLE_ENTRY){0};
}

STR SYMBOL_TABLE_getNameFromCurrent(void) {
  return SYMBOL_TABLE_getNameFromStart(SYMBOL_TABLE_getCurrentScopeIndex());
}

STR SYMBOL_TABLE_getNameFromStart(int start) {
  uint32_t current = start;
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    if (scope.moduleName != EMPTY_STRING) {
      return scope.moduleName;
    }
    current = scope.parent;
  }
  return EMPTY_STRING;
}
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrent(STR name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  TYPE_ID type = 0;
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    SYMBOL_TABLE_ENTRY entry = hmgets(scope.table, name);
    if (entry.defined) {
      if (entry.entryType == SYMBOL_TYPE_SHADOW) {
        type = entry.typeIndex;
      } else {
        if (type != 0) {
          entry.typeIndex = type;
        }
        return entry;
      }
    }
    current = scope.parent;
  }
  return (SYMBOL_TABLE_ENTRY){0};
}

void SYMBOL_TABLE_report(void) {
  printf("SYMBOL TABLE - Report:\n");
  for (int i = 0; i < hmlen(scopes); i++) {
    SYMBOL_TABLE_SCOPE scope = scopes[i];
    printf("Scope %u (parent %u):\n", scope.key, scope.parent);
    if (scope.scopeType == SCOPE_TYPE_MODULE && scope.moduleName != EMPTY_STRING) {
      printf(" (module: %s):\n", CHARS(scope.moduleName));
    }
    printf(" (table size %u):\n", scope.tableSize);
    if (scope.scopeType == SCOPE_TYPE_FUNCTION) {
      printf(" (count %u):\n", scope.tableAllocationCount);
      printf(" (stack required %u):\n", scope.tableAllocationSize);
    }
    for (int j = 0; j < hmlen(scope.table); j++) {
      SYMBOL_TABLE_ENTRY entry = scope.table[j];

      printf("%s - ", CHARS(entry.key));
      printf("%s - ", CHARS(TYPE_get(entry.typeIndex).name));
      switch (entry.entryType) {
        case SYMBOL_TYPE_UNKNOWN: { printf("UNKNOWN"); break; }
        case SYMBOL_TYPE_FUNCTION: { printf("FUNCTION"); break; }
        case SYMBOL_TYPE_PARAMETER: { printf("PARAMETER"); break; }
        case SYMBOL_TYPE_VARIABLE: { printf("VARIABLE"); break; }
        case SYMBOL_TYPE_CONSTANT: { printf("CONSTANT"); break; }
      }
      if (entry.elementCount > 0) {
        printf("(%i elements)", entry.elementCount);
      }
      printf("\n");

    }
    printf("End Scope %u.\n\n", scope.key);
    printf("---------------------------\n");
  }
}

bool SYMBOL_TABLE_nameScope(STR name) {
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getCurrentScope();
  if (scope.moduleName != EMPTY_STRING) {
    return true;
  }
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && scopes[i].moduleName == name) {
      return false;
    }
  }

  scope.moduleName = name;
  hmputs(scopes, scope);
  return true;
}

SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScopeByName(STR name) {
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && scopes[i].moduleName == name) {
      return scopes[i];
    }
  }
  return (SYMBOL_TABLE_SCOPE){};
}
int SYMBOL_TABLE_getScopeIndexByName(STR name) {
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && scopes[i].moduleName == name) {
      return scopes[i].key;
    }
  }
  return -1;
}

void SYMBOL_TABLE_free(void) {
  // SYMBOL_TABLE_closeScope();
  for (int i=0; i < hmlen(scopes); i++) {
    SYMBOL_TABLE_SCOPE scope = scopes[i];
    // TODO: free the param list in each scope entry
    hmfree(scope.table);
  }
  hmfree(scopes);
  arrfree(scopeStack);
  arrfree(leafScopes);
}
