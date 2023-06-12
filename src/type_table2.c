
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

// typedef struct { int key; bool value; } TYPE_VISIT_SET;
// typedef struct { int size; bool error; } TYPE_TABLE_RESULT;
// struct { char *key; TYPE_ID value; }* aliasTable = NULL;

TYPE_ENTRY* typeTable = NULL;

TYPE_ENTRY* TYPE_TABLE_init(void) {
  return typeTable;
}

void TYPE_TABLE_free(void) {
  // Do nothing for now, we'll clean this up later
}

TYPE_ID TYPE_declare(STRING* module, STRING* name) {
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
  if (index > arrlen(typeTable) - 1) {
    return 0;
  }

  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].entryType = entryType;
  typeTable[index].fields = fields;

  return index;
}

TYPE_ID TYPE_registerPrimitive(STRING* name) {
  TYPE_ID id = arrlen(typeTable);
  arrput(typeTable, ((TYPE_ENTRY){
        .index = id,
        .module = NULL,
        .name = name,
        .entryType = ENTRY_TYPE_PRIMITIVE,
        .fields = NULL,
        .status = STATUS_COMPLETE
  }));
  return id;
}

TYPE_ENTRY TYPE_get(TYPE_ID index) {
  return typeTable[index];
}

TYPE_ENTRY TYPE_getByName(char* module, char* name) {
  return typeTable[0];
}

bool TYPE_hasParent(TYPE_ID index) {
  TYPE_ENTRY entry = typeIndex[index];
  if (entry.entryType != ENTRY_TYPE_ARRAY && entry.entryType != ENTRY_TYPE_POINTER) {
    return false;
  }

  return true;
}

TYPE_ID TYPE_getParent(TYPE_ID index) {
  if (TYPE_hasParent(index)) {
    return 0;
  }
  TYPE_ENTRY entry = typeIndex[index];
  return entry.fields[0].typeIndex;
}

void TYPE_TABLE_report(void) {

}

