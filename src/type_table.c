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

typedef struct { int key; bool value; } TYPE_VISIT_SET;
typedef struct { int size; bool error; } TYPE_TABLE_RESULT;

TYPE_TABLE_ENTRY* typeTable = NULL;
struct { char *key; int value; }* aliasTable = NULL;


int TYPE_TABLE_defineCallable(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields, int returnType) {
  if (index > arrlen(typeTable) - 1) {
    return 0;
  }
  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].entryType = ENTRY_TYPE_FUNCTION;
  typeTable[index].byteSize = 2;
  typeTable[index].parent = parent;
  typeTable[index].fields = fields;
  typeTable[index].returnType = returnType;

  return index;
}
int TYPE_TABLE_define(int index, enum TYPE_TABLE_ENTRY_TYPE entryType,  size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  if (index > arrlen(typeTable) - 1) {
    return 0;
  }

  typeTable[index].entryType = entryType;
  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].parent = parent;
  typeTable[index].fields = fields;

  return index;
}

int TYPE_TABLE_declare(STRING* name) {
  int i = shget(aliasTable, name->chars);
  if (i > 0) {
    return i;
  }

  shput(aliasTable, name->chars, arrlen(typeTable));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = name, .status = STATUS_DECLARED, .fields = NULL, .byteSize = 0, .parent = 0, .entryType = ENTRY_TYPE_UNKNOWN }));
  return arrlen(typeTable) - 1;
}

int TYPE_TABLE_registerPrimitive(STRING* name, size_t size) {
  if (name != NULL) {
    int i = shget(aliasTable, name->chars);
    if (i > 0) {
      return i;
    }
    shput(aliasTable, name->chars, arrlen(typeTable));
  } else if (arrlen(typeTable) > 1) {
    return 0;
  }

  arrput(typeTable, ((TYPE_TABLE_ENTRY){
        .name = name,
        .entryType = ENTRY_TYPE_PRIMITIVE,
        .byteSize = size,
        .parent = 0,
        .fields = NULL,
        .status = STATUS_COMPLETE
  }));
  return arrlen(typeTable) - 1;
}

int TYPE_TABLE_registerType(STRING* name, enum TYPE_TABLE_ENTRY_TYPE entryType,  size_t size, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  int i = shget(aliasTable, name->chars);
  if (i > 0) {
    return i;
  }
  if (name != NULL) {
    shput(aliasTable, name->chars, arrlen(typeTable));
  }
  arrput(typeTable, ((TYPE_TABLE_ENTRY){
        .name = name,
        .entryType = entryType,
        .byteSize = size,
        .parent = parent,
        .fields = fields,
        .status = STATUS_COMPLETE
  }));
  return arrlen(typeTable) - 1;
}

void TYPE_TABLE_free() {
  for (int i = 0; i < arrlen(typeTable); i++) {
    arrfree(typeTable[i].fields);
    STRING_free(typeTable[i].name);
  }
  arrfree(typeTable);
  shfree(aliasTable);
}

TYPE_TABLE_ENTRY* TYPE_TABLE_init() {
  sh_new_arena(aliasTable);
  shdefault(aliasTable, 0);
  shput(aliasTable, "uint8", 3);
  //shput(aliasTable, "char", 3);
  shput(aliasTable, "int8", 4);
  shput(aliasTable, "uint16", 5);
  shput(aliasTable, "ptr", 5);
  shput(aliasTable, "int16", 6);

  TYPE_TABLE_registerPrimitive(NULL, 0);
  TYPE_TABLE_registerPrimitive(createString("void"), 0);
  TYPE_TABLE_registerPrimitive(createString("bool"), 1);
  TYPE_TABLE_registerPrimitive(createString("u8"), 1);
  TYPE_TABLE_registerPrimitive(createString("i8"), 1);
  TYPE_TABLE_registerPrimitive(createString("u16"), 2);
  TYPE_TABLE_registerPrimitive(createString("i16"), 2);
  TYPE_TABLE_registerPrimitive(createString("literal"), 4);
  TYPE_TABLE_registerPrimitive(createString("string"), 2);
  TYPE_TABLE_registerPrimitive(createString("fn"), 2);
  TYPE_TABLE_registerPrimitive(createString("char"), 1);

  return typeTable;
}


static TYPE_TABLE_RESULT TYPE_TABLE_calculateTypeSize(int typeIndex, TYPE_VISIT_SET* visitSet) {
  TYPE_TABLE_ENTRY* entry = typeTable + typeIndex;
  if (entry->entryType == ENTRY_TYPE_PRIMITIVE || entry->status == STATUS_COMPLETE) {
    return (TYPE_TABLE_RESULT){ entry->byteSize, false };
  }
  hmput(visitSet, typeIndex, true);

  size_t total = 0;
  if (entry->parent == 0) {
    for (int j = 0; j < arrlen(entry->fields); j++) {
      TYPE_TABLE_FIELD_ENTRY field = entry->fields[j];
      if (hmgeti(visitSet, field.typeIndex) != -1) {
        printf("Types cannot be recursively defined.\n");
        return (TYPE_TABLE_RESULT){ 0, true };
      }

      TYPE_TABLE_ENTRY fieldType = typeTable[field.typeIndex];
      if (fieldType.entryType == ENTRY_TYPE_PRIMITIVE) {
        total += fieldType.byteSize;
      } else {
        if (fieldType.status == STATUS_COMPLETE) {
          total += fieldType.byteSize;
        } else {
          TYPE_TABLE_RESULT result = TYPE_TABLE_calculateTypeSize(field.typeIndex, visitSet);
          if (result.error) {
            return result;
          }
          total += result.size;
        }
      }
    }
  } else {
    total += typeTable[entry->parent].byteSize;
  }
  entry->byteSize = total;
  entry->status = STATUS_COMPLETE;
  return (TYPE_TABLE_RESULT){ total, false };
}

bool TYPE_TABLE_calculateSizes() {
  for (int i = 1; i < arrlen(typeTable); i++) {
    TYPE_VISIT_SET* visited = NULL;
    TYPE_TABLE_RESULT result = TYPE_TABLE_calculateTypeSize(i, visited);
    if (result.error) {
      return false;
    }
  }
  return true;
}

int TYPE_TABLE_lookup(STRING* name) {
  if (name == NULL || name->chars == NULL) {
    return 0;
  }
  int index = shget(aliasTable, name->chars);
  return index;
}

void TYPE_TABLE_report() {
  printf("-------- TYPE TABLE (%zu)-----------\n", arrlen(typeTable));
  for (int i = 1; i < arrlen(typeTable); i++) {
    TYPE_TABLE_ENTRY* entry = typeTable + i;
    printf("%s - %s | %zu bytes", entry->name->chars, entry->status == STATUS_COMPLETE ? "complete" : "incomplete", entry->byteSize);
    printf("\n");
  }
  printf("-------------------------------\n");
}
