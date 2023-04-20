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

TYPE_TABLE_ENTRY* TYPE_TABLE_init() {
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = NULL, .parent = 0, .byteSize = 0 })); // NONE
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("void"), .parent = 0, .byteSize = 0 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("int8"), .parent = 0 , .byteSize = 2}));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("int16"), .parent = 0, .byteSize = 2 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("uint8"), .parent = 0, .byteSize = 1 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("uint8"), .parent = 0, .byteSize = 1 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("bool"), .parent = 0, .byteSize = 1 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("string"), .parent = 0, .byteSize = 2 }));
  arrput(typeTable, ((TYPE_TABLE_ENTRY){ .name = createString("ptr"), .parent = 0, .byteSize = 2 }));

  return typeTable;
}

void TYPE_TABLE_free() {
  for (int i = 0; i < arrlen(typeTable); i++) {
    STRING_free(typeTable[i].name);
  }
  arrfree(typeTable);
}
