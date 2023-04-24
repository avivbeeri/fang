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

enum TYPE_TABLE_ENTRY_STATUS {
  STATUS_DECLARED,
  STATUS_DEFINED,
  STATUS_COMPLETE
};

#include "memory.h"
typedef struct TYPE_TABLE_FIELD_ENTRY {
  STRING* name;
  int typeIndex;
} TYPE_TABLE_FIELD_ENTRY;

typedef struct TYPE_TABLE_ENTRY {
  STRING* name;
  enum TYPE_TABLE_ENTRY_STATUS status;
  bool primitive;
  size_t parent;
  size_t byteSize;
  TYPE_TABLE_FIELD_ENTRY* fields;
} TYPE_TABLE_ENTRY;

extern TYPE_TABLE_ENTRY* typeTable;

TYPE_TABLE_ENTRY* TYPE_TABLE_init(void);
int TYPE_TABLE_define(int index, size_t parent, TYPE_TABLE_FIELD_ENTRY* fields);
int TYPE_TABLE_declare(STRING* name);
int TYPE_TABLE_registerType(STRING* name, size_t size, size_t parent);
void TYPE_TABLE_free(void);
bool TYPE_TABLE_calculateSizes();
int TYPE_TABLE_lookup(STRING* name);

void TYPE_TABLE_report();

#endif
