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

#include <stdio.h>
#include "common.h"
#include "memory.h"
#include "type_table.h"
#include "symbol_table.h"
#include <math.h>

uint32_t scopeId = 1;
int* scopeStack = NULL;

SYMBOL_TABLE_SCOPE* scopes = NULL;

void SYMBOL_TABLE_openScope(SYMBOL_TABLE_SCOPE_TYPE scopeType) {
  uint32_t parent = 0;
  if (scopeStack != NULL) {
    parent = scopeStack[arrlen(scopeStack) - 1];
  }
  hmputs(scopes, ((SYMBOL_TABLE_SCOPE){
        scopeId,
        parent,
        NULL,
        scopeType,
        NULL,
        0,
        0,
        0,
        0,
        0,
        0
  }));
  arrput(scopeStack, scopeId);
  scopeId++;
}

void SYMBOL_TABLE_pushScope(int index) {
  arrput(scopeStack, index);
}
void SYMBOL_TABLE_popScope() {
  arrdel(scopeStack, arrlen(scopeStack) - 1);
}

bool SYMBOL_TABLE_scopeHas(STRING* name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    if (shgeti(scope.table, name->chars) != -1) {
      return true;
    }
    current = scope.parent;
  }
  return false;
}

void SYMBOL_TABLE_closeScope() {
  uint32_t current = SYMBOL_TABLE_getCurrentScopeIndex();
  SYMBOL_TABLE_SCOPE closingScope = SYMBOL_TABLE_getScope(current);
  SYMBOL_TABLE_SCOPE parent = SYMBOL_TABLE_getScope(closingScope.parent);

  uint32_t scopeCount = hmlen(closingScope.table);
  /*
  uint32_t scopeSize = 0;
  for (int i = 0; i < scopeCount; i++) {
    SYMBOL_TABLE_ENTRY entry = closingScope.table[i];
    if (entry.defined) {
      scopeSize += typeTable[entry.typeIndex].byteSize;
    }
  }
  closingScope.tableAllocationSize = scopeSize + closingScope.nestedSize;
  parent.nestedSize = fmax(parent.nestedSize, scopeSize);
  */


  closingScope.tableAllocationCount = scopeCount + closingScope.nestedCount;
  parent.nestedCount = fmax(parent.nestedCount, closingScope.tableAllocationCount);

  hmputs(scopes, closingScope);
  hmputs(scopes, parent);
  arrdel(scopeStack, arrlen(scopeStack) - 1);
}

uint32_t SYMBOL_TABLE_getCurrentScopeIndex() {
  if (arrlen(scopeStack) == 0) {
    return 0;
  }
  return scopeStack[arrlen(scopeStack) - 1];
}

void SYMBOL_TABLE_init(void) {
  SYMBOL_TABLE_openScope(SCOPE_TYPE_INVALID);
}

void SYMBOL_TABLE_declare(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex) {
  uint32_t scopeIndex = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeIndex);

  SYMBOL_TABLE_ENTRY entry = {
    .key = name->chars,
    .defined = true,
    .entryType = type,
    .status = SYMBOL_TABLE_STATUS_DECLARED,
    .typeIndex = typeIndex,
    .scopeIndex = scopeIndex,
    .constantIndex = 0
  };
  shputs(scope.table, entry);
  hmputs(scopes, scope);
}
void SYMBOL_TABLE_putFn(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex) {
  uint32_t scopeIndex = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, scopeIndex);

  uint32_t offset = 0;
  if (type == SYMBOL_TYPE_VARIABLE) {
    offset = scope.localOffset;
    scope.localOffset += typeTable[typeIndex].byteSize;
  } else if (type == SYMBOL_TYPE_PARAMETER) {
    offset = scope.paramOffset;
    scope.paramOffset += typeTable[typeIndex].byteSize;
  }

  SYMBOL_TABLE_ENTRY entry = {
    .key = name->chars,
    .entryType = type,
    .defined = true,
    .status = SYMBOL_TABLE_STATUS_DEFINED,
    .typeIndex = typeIndex,
    .scopeIndex = scopeIndex,
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
  shputs(scope.table, entry);
  hmputs(scopes, scope);
}

void SYMBOL_TABLE_put(STRING* name, SYMBOL_TYPE type, uint32_t typeIndex) {
  SYMBOL_TABLE_putFn(name, type, typeIndex);
}

SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getCurrentScope() {
  return hmgets(scopes, SYMBOL_TABLE_getCurrentScopeIndex());
}
SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScope(uint32_t scope) {
  return hmgets(scopes, scope);
}

SYMBOL_TABLE_ENTRY SYMBOL_TABLE_get(uint32_t scopeIndex, STRING* name) {
  uint32_t current = scopeIndex;
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    SYMBOL_TABLE_ENTRY entry = shgets(scope.table, name->chars);
    if (entry.defined) {
      return entry;
    }
    current = scope.parent;
  }
  return (SYMBOL_TABLE_ENTRY){0};
}
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrentOnly(STRING* name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
  SYMBOL_TABLE_ENTRY entry = shgets(scope.table, name->chars);
  if (entry.defined) {
    return entry;
  }
  return (SYMBOL_TABLE_ENTRY){0};
}
STRING* SYMBOL_TABLE_getNameFromStart(int start) {
  uint32_t current = start;
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    if (scope.moduleName != NULL) {
      return scope.moduleName;
    }
    current = scope.parent;
  }
  return NULL;
}
SYMBOL_TABLE_ENTRY SYMBOL_TABLE_getCurrent(STRING* name) {
  uint32_t current = scopeStack[arrlen(scopeStack) - 1];
  while (current > 0) {
    SYMBOL_TABLE_SCOPE scope = hmgets(scopes, current);
    SYMBOL_TABLE_ENTRY entry = shgets(scope.table, name->chars);
    if (entry.defined) {
      return entry;
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
    if (scope.scopeType == SCOPE_TYPE_MODULE && scope.moduleName != NULL) {
      printf(" (module: %s):\n", scope.moduleName->chars);
    }
    if (scope.scopeType == SCOPE_TYPE_FUNCTION) {
      printf(" (stack required %u):\n", scope.tableAllocationCount);
    }
    for (int j = 0; j < hmlen(scope.table); j++) {
      SYMBOL_TABLE_ENTRY entry = scope.table[j];

      printf("%s - ", entry.key);
      printf("%s - ", typeTable[entry.typeIndex].name->chars);
      switch (entry.entryType) {
        case SYMBOL_TYPE_UNKNOWN: { printf("UNKNOWN"); break; }
        case SYMBOL_TYPE_MODULE: { printf("MODULE"); break; }
        case SYMBOL_TYPE_KEYWORD: { printf("KEYWORD"); break; }
        case SYMBOL_TYPE_FUNCTION: { printf("FUNCTION"); break; }
        case SYMBOL_TYPE_PARAMETER: { printf("PARAMETER"); break; }
        case SYMBOL_TYPE_VARIABLE: { printf("VARIABLE"); break; }
        case SYMBOL_TYPE_CONSTANT: { printf("CONSTANT"); break; }
      }
      printf("\n");

    }
    printf("End Scope %u.\n\n", scope.key);
    printf("---------------------------\n");
  }
}

bool SYMBOL_TABLE_nameScope(STRING* name) {
  SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getCurrentScope();
  if (scope.moduleName != NULL) {
    return true;
  }
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && STRING_equality(scopes[i].moduleName, name)) {
      return false;
    }
  }

  scope.moduleName = name;
  hmputs(scopes, scope);
  return true;
}

SYMBOL_TABLE_SCOPE SYMBOL_TABLE_getScopeByName(STRING* name) {
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && STRING_equality(scopes[i].moduleName, name)) {
      return scopes[i];
    }
  }
  return (SYMBOL_TABLE_SCOPE){};
}
int SYMBOL_TABLE_getScopeIndexByName(STRING* name) {
  for (int i = 0; i < hmlen(scopes); i++) {
    if (scopes[i].scopeType != SCOPE_TYPE_INVALID && STRING_equality(scopes[i].moduleName, name)) {
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
    shfree(scope.table);
    STRING_free(scope.moduleName);
  }
  hmfree(scopes);
  arrfree(scopeStack);
}
