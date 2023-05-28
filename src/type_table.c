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


int TYPE_TABLE_defineType(int index, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields, int returnType) {
  if (index > arrlen(typeTable) - 1) {
    return 0;
  }
  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].entryType = entryType;
  typeTable[index].parent = parent;
  typeTable[index].fields = fields;
  typeTable[index].returnType = returnType;

  return index;
}

int TYPE_TABLE_defineCallable(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields, int returnType) {
  return TYPE_TABLE_defineType(index, ENTRY_TYPE_FUNCTION, parent, fields, returnType);
}

int TYPE_TABLE_define(int index, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  return TYPE_TABLE_defineType(index, entryType, parent, fields, 0);
}

int TYPE_TABLE_declare(STRING* name) {
  int i = shget(aliasTable, name->chars);
  if (i > 0) {
    return i;
  }

  shput(aliasTable, name->chars, arrlen(typeTable));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){
    .name = name,
    .status = STATUS_DECLARED,
    .fields = NULL,
    .byteSize = 0,
    .parent = 0,
    .entryType = ENTRY_TYPE_UNKNOWN
  }));
  return arrlen(typeTable) - 1;
}

int TYPE_TABLE_registerPrimitive(STRING* name) {
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
        .byteSize = 0,
        .parent = 0,
        .fields = NULL,
        .status = STATUS_COMPLETE
  }));
  return arrlen(typeTable) - 1;
}

int TYPE_TABLE_registerType(STRING* name, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
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
        .parent = parent,
        .fields = fields,
        .status = STATUS_DEFINED
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
  shput(aliasTable, "int8", 4);
  shput(aliasTable, "uint16", 5);
  shput(aliasTable, "int16", 6);

  TYPE_TABLE_registerPrimitive(NULL);
  TYPE_TABLE_registerPrimitive(createString("void"));
  TYPE_TABLE_registerPrimitive(createString("bool"));
  TYPE_TABLE_registerPrimitive(createString("u8"));
  TYPE_TABLE_registerPrimitive(createString("i8"));
  TYPE_TABLE_registerPrimitive(createString("u16"));
  TYPE_TABLE_registerPrimitive(createString("i16"));
  TYPE_TABLE_registerPrimitive(createString("number"));
  TYPE_TABLE_registerType(createString("string"), ENTRY_TYPE_ARRAY, 10, NULL); // 10 = char index
  TYPE_TABLE_registerPrimitive(createString("fn"));
  TYPE_TABLE_registerPrimitive(createString("char"));
  TYPE_TABLE_registerPrimitive(createString("ptr"));

  return typeTable;
}

int TYPE_TABLE_lookup(char* name) {
  if (name == NULL) {
    return 0;
  }
  int index = shget(aliasTable, name);
  return index;
}

int TYPE_TABLE_lookupWithString(STRING* name) {
  return TYPE_TABLE_lookup(name->chars);
}

bool TYPE_TABLE_setPrimitiveSize(char* name, int size) {
  int typeIndex = TYPE_TABLE_lookup(name);
  TYPE_TABLE_ENTRY* entry = typeTable + typeIndex;
  if (entry->entryType != ENTRY_TYPE_PRIMITIVE) {
    return false;
  }
  entry->byteSize = size;
  return true;
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
    // TODO revisit this
    if (entry->entryType == ENTRY_TYPE_ARRAY || entry->entryType == ENTRY_TYPE_POINTER) {
      total += typeTable[TYPE_TABLE_lookup("ptr")].byteSize;
    } else {
      total += typeTable[entry->parent].byteSize;
    }
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

void TYPE_TABLE_report() {
  printf("-------- TYPE TABLE (%zu)-----------\n", arrlen(typeTable));
  for (int i = 1; i < arrlen(typeTable); i++) {
    TYPE_TABLE_ENTRY* entry = typeTable + i;
    printf("%s - %s | %zu bytes", entry->name->chars, entry->status == STATUS_COMPLETE ? "complete" : "incomplete", entry->byteSize);
    printf("\n");
  }
  printf("-------------------------------\n");
}
