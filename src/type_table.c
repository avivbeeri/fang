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

#include "common.h"
#include "memory.h"
#include "type_table.h"

TYPE_TABLE_ENTRY* typeTable = NULL;


int TYPE_TABLE_define(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields, size_t size) {
  typeTable[index].defined = true;
  typeTable[index].parent = parent;
  typeTable[index].fields = fields;
  typeTable[index].byteSize = size;

  return index;
}

int TYPE_TABLE_declare(STRING* name) {
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = name, .defined = false, .fields = NULL, .byteSize = 0, .parent = 0 }));
  return arrlen(typeTable) - 1;
}
int TYPE_TABLE_register(STRING* name, size_t size, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields) {
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = name, .byteSize = size, .parent = parent, .defined = true, .fields = fields }));
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
