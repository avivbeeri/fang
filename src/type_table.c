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

TYPE_TABLE_ENTRY* typeTable = NULL;


int TYPE_TABLE_define(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  typeTable[index].status = STATUS_DEFINED;
  typeTable[index].parent = parent;
  typeTable[index].fields = fields;

  return index;
}

int TYPE_TABLE_declare(STRING* name) {
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = name, .status = STATUS_DECLARED, .fields = NULL, .byteSize = 0, .parent = 0 }));
  return arrlen(typeTable) - 1;
}

int TYPE_TABLE_register(STRING* name, size_t size, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  arrput(typeTable, ((TYPE_TABLE_ENTRY){
        .name = name,
        .byteSize = size,
        .parent = parent,
        .status = STATUS_COMPLETE,
        .fields = fields,
        .primitive = true
  }));
  return arrlen(typeTable) - 1;
}

void TYPE_TABLE_free() {
  for (int i = 0; i < arrlen(typeTable); i++) {
    arrfree(typeTable[i].fields);
    STRING_free(typeTable[i].name);
  }
  arrfree(typeTable);
}

TYPE_TABLE_ENTRY* TYPE_TABLE_init() {
  TYPE_TABLE_register(NULL, 0, 0, NULL);
  TYPE_TABLE_register(createString("void"), 0, 0, NULL);
  TYPE_TABLE_register(createString("bool"), 1, 0, NULL);
  TYPE_TABLE_register(createString("uint8"), 1, 0, NULL);
  TYPE_TABLE_register(createString("int8"), 1, 0, NULL);
  TYPE_TABLE_register(createString("uint16"), 2, 0, NULL);
  TYPE_TABLE_register(createString("int16"), 2, 0, NULL);
  TYPE_TABLE_register(createString("string"), 2, 0, NULL);
  TYPE_TABLE_register(createString("ptr"), 2, 0, NULL);

  return typeTable;
}

typedef struct { int key; bool value; } INT_SET;

static int TYPE_TABLE_calculateTypeSize(int typeIndex, INT_SET* visitSet) {
  TYPE_TABLE_ENTRY* entry = typeTable + typeIndex;
  if (entry->primitive || entry->status == STATUS_COMPLETE) {
    return entry->byteSize;
  }
  hmput(visitSet, typeIndex, true);

  size_t total = 0;
  for (int j = 0; j < arrlen(entry->fields); j++) {
    TYPE_TABLE_FIELD_ENTRY field = entry->fields[j];
    if (hmgeti(visitSet, field.typeIndex) != -1) {
      printf("Types cannot be recursively defined.\n");
      return 0;
    }

    TYPE_TABLE_ENTRY fieldType = typeTable[field.typeIndex];
    if (fieldType.primitive) {
      total += fieldType.byteSize;
    } else {
      if (fieldType.status == STATUS_COMPLETE) {
        total += fieldType.byteSize;
      } else {
        size_t size = TYPE_TABLE_calculateTypeSize(field.typeIndex, visitSet);
        if (size == 0) {
          return 0;
        }
        total += size;
      }
    }
  }
  if (total > 0) {
    entry->byteSize = total;
    entry->status = STATUS_COMPLETE;
  }
  return total;
}

bool TYPE_TABLE_calculateSizes() {
  for (int i = 1; i < arrlen(typeTable); i++) {
    INT_SET* visited = NULL;
    size_t size = TYPE_TABLE_calculateTypeSize(i, visited);
    if (!typeTable[i].primitive && size == 0) {
      return false;
    }
  }
  return true;
}

int TYPE_TABLE_lookup(STRING* name) {
  // We skip type 0 because 0 is the not-exist option.
  for (int i = 1; i < arrlen(typeTable); i++) {
    if (STRING_equality(name, typeTable[i].name)) {
      return i;
    }
  }
  return 0;
}

void TYPE_TABLE_report() {
  for (int i = 1; i < arrlen(typeTable); i++) {
    TYPE_TABLE_ENTRY* entry = typeTable + i;
    printf("%s - %s | %zu bytes", entry->name->chars, entry->status == STATUS_COMPLETE ? "complete" : "incomplete", entry->byteSize);
    printf("\n");
  }
}
