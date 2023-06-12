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

#ifndef type_table_h
#define type_table_h
#include "memory.h"
#include "symbol_table.h"

enum TYPE_TABLE_ENTRY_STATUS {
  STATUS_DECLARED,
  STATUS_DEFINED,
  STATUS_COMPLETE,
  STATUS_EXTERNAL
};

typedef struct TYPE_TABLE_FIELD_ENTRY {
  STRING* name;
  uint32_t typeIndex;
  uint32_t elementCount;
//  This is needed to derive something, but I'd prefer it not to be
//  SYMBOL_KIND kind;
} TYPE_TABLE_FIELD_ENTRY;


enum TYPE_TABLE_ENTRY_TYPE {
  ENTRY_TYPE_UNKNOWN,
  ENTRY_TYPE_PRIMITIVE,
  ENTRY_TYPE_POINTER,
  ENTRY_TYPE_FUNCTION,
  ENTRY_TYPE_ARRAY,
  ENTRY_TYPE_RECORD,
  ENTRY_TYPE_UNION
};

typedef struct TYPE_TABLE_ENTRY {
  STRING* name;
  enum TYPE_TABLE_ENTRY_STATUS status;
  enum TYPE_TABLE_ENTRY_TYPE entryType;
  // Pointers and arrays have "parents" (really the pointed type)
  size_t parent;
  size_t container;
  size_t byteSize;
  // Functions have a return type
  int returnType;
  TYPE_TABLE_FIELD_ENTRY* fields;
} TYPE_TABLE_ENTRY;

extern TYPE_TABLE_ENTRY* typeTable;

TYPE_TABLE_ENTRY* TYPE_TABLE_init(void);
int TYPE_TABLE_define(int index, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields);
int TYPE_TABLE_defineCallable(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields, int returnType);
int TYPE_TABLE_declare(STRING* name);
int TYPE_TABLE_registerPrimitive(STRING* name);
bool TYPE_TABLE_setPrimitiveSize(char* name, int size);
int TYPE_TABLE_registerType(STRING* name, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields);
int TYPE_TABLE_registerArray(STRING* name, enum TYPE_TABLE_ENTRY_TYPE entryType, size_t parent);
void TYPE_TABLE_free(void);
bool TYPE_TABLE_calculateSizes();

void TYPE_TABLE_report();
TYPE_TABLE_ENTRY TYPE_TABLE_get(TYPE_ID index);
int TYPE_TABLE_lookup(char* name);
int TYPE_TABLE_lookupWithString(STRING* name);
#endif
