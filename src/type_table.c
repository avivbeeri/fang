
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

static TYPE_ENTRY* typeTable = NULL;
struct { STR key; bool value; }* moduleSet = NULL;

TYPE_ENTRY* TYPE_TABLE_init(void) {
  TYPE_registerPrimitive(NULL);
  TYPE_registerPrimitive("void");
  TYPE_registerPrimitive("bool");
  TYPE_registerPrimitive("u8");
  TYPE_registerPrimitive("i8");
  TYPE_registerPrimitive("u16");
  TYPE_registerPrimitive("i16");
  TYPE_registerPrimitive("number");
  int strIndex = TYPE_declare(EMPTY_STRING, STR_create("string"));
  TYPE_FIELD_ENTRY* subType = NULL;
  arrput(subType, ((TYPE_FIELD_ENTRY){ 10, EMPTY_STRING, 0 }));
  TYPE_define(strIndex, ENTRY_TYPE_POINTER, subType);
  TYPE_registerPrimitive("fn");
  TYPE_registerPrimitive("char");
  TYPE_ID id = TYPE_declare(STR_create("sys"), STR_create("ptr"));
  TYPE_define(id, ENTRY_TYPE_PRIMITIVE, NULL);
  return typeTable;
}

void TYPE_TABLE_free(void) {
  // Do nothing for now, we'll clean this up later
  for (int i = 0; i < arrlen(typeTable); i++) {
    arrfree(typeTable[i].fields);
  }
  arrfree(typeTable);
  hmfree(moduleSet);
}

TYPE_ID TYPE_declare(STR module, STR name) {
  if (module != EMPTY_STRING) {
    hmput(moduleSet, module, true);
  }
  if (TYPE_getIdByName(module, name) != 0) {
    return TYPE_getIdByName(module, name);
  }
  TYPE_ID id = arrlen(typeTable);
  arrput(typeTable, ((TYPE_ENTRY){
    .index = id,
    .module = module,
    .name = name,
    .entryType = ENTRY_TYPE_UNKNOWN,
    .fields = NULL,
    .status = STATUS_DECLARED,
  }));
  return id;
}

TYPE_ID TYPE_define(TYPE_ID index, TYPE_ENTRY_TYPE entryType, TYPE_FIELD_ENTRY* fields) {
  if (typeTable[index].status != STATUS_DECLARED) {
    // printf("duplicate definition: %s\n", typeTable[index].name->chars);
    arrfree(fields);
    return index;
  }

  if (index > arrlen(typeTable) - 1) {
    return 0;
  }

  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].entryType = entryType;
  typeTable[index].fields = fields;

  return index;
}

TYPE_ID TYPE_registerPrimitive(char* name) {
  TYPE_ID id = arrlen(typeTable);
  if (name == NULL) {
    arrput(typeTable, ((TYPE_ENTRY){
          .index = id,
          .module = EMPTY_STRING,
          .name = EMPTY_STRING,
          .entryType = ENTRY_TYPE_PRIMITIVE,
          .fields = NULL,
          .status = STATUS_COMPLETE
    }));
  } else {
    arrput(typeTable, ((TYPE_ENTRY){
          .index = id,
          .module = EMPTY_STRING,
          .name = STR_create(name),
          .entryType = ENTRY_TYPE_PRIMITIVE,
          .fields = NULL,
          .status = STATUS_COMPLETE
    }));
  }
  return id;
}

TYPE_ENTRY TYPE_get(TYPE_ID index) {
  return typeTable[index];
}

TYPE_ID TYPE_getIdByName(STR module, STR name) {
  int moduleCount = hmlen(moduleSet);
  for (int i = 1; i < arrlen(typeTable); i++) {
    TYPE_ENTRY entry = typeTable[i];
    if (moduleCount > 0) {
      if (entry.module != module) {
        continue;
      }
    }

    if (entry.name == name) {
      return i;
    }
  }
  return 0;
}

bool TYPE_hasParent(TYPE_ID index) {
  TYPE_ENTRY entry = typeTable[index];
  if (entry.entryType != ENTRY_TYPE_ARRAY && entry.entryType != ENTRY_TYPE_POINTER) {
    return false;
  }

  return true;
}

TYPE_ID TYPE_getParentId(TYPE_ID index) {
  if (!TYPE_hasParent(index)) {
    return 0;
  }
  TYPE_ENTRY entry = typeTable[index];
  return entry.fields[0].typeIndex;
}

TYPE_ENTRY TYPE_getParent(TYPE_ID index) {
  TYPE_ENTRY entry = TYPE_get(index);
  return TYPE_get(entry.fields[0].typeIndex);
}

TYPE_ENTRY_TYPE TYPE_getKind(TYPE_ID type) {
  return TYPE_get(type).entryType;
}

int TYPE_getTag(TYPE_ID unionType, TYPE_ID subType) {

  TYPE_FIELD_ENTRY* fields = TYPE_get(unionType).fields;
  int tag = -1;
  for (int i = 0; i < arrlen(fields); i++) {
    if (fields[i].typeIndex == subType) {
      tag = i;
      break;
    }
  }
  return tag;
}


size_t TYPE_TABLE_total(void) {
  return arrlen(typeTable);
}

void printKind(int kind) {
  switch (TYPE_getKind(kind)) {
    case ENTRY_TYPE_PRIMITIVE: printf("primitive"); break;
    case ENTRY_TYPE_POINTER: printf("pointer"); break;
    case ENTRY_TYPE_RECORD: printf("record"); break;
    case ENTRY_TYPE_UNION: printf("union"); break;
    case ENTRY_TYPE_ARRAY: printf("array"); break;
    case ENTRY_TYPE_FUNCTION: printf("fun"); break;
    default:
    case ENTRY_TYPE_UNKNOWN: printf("unknown"); break;
  }
}

