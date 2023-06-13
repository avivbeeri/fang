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

typedef enum TYPE_ENTRY_STATUS {
  STATUS_UNKNOWN,
  STATUS_DECLARED,
  STATUS_DEFINED,
  STATUS_COMPLETE,
  STATUS_EXTERNAL
} TYPE_ENTRY_STATUS;

typedef enum TYPE_ENTRY_TYPE {
  ENTRY_TYPE_UNKNOWN,
  ENTRY_TYPE_PRIMITIVE,
  ENTRY_TYPE_POINTER,
  ENTRY_TYPE_FUNCTION,
  ENTRY_TYPE_ARRAY,
  ENTRY_TYPE_RECORD,
  ENTRY_TYPE_UNION
} TYPE_ENTRY_TYPE;

typedef uint32_t TYPE_ID;

typedef struct TYPE_FIELD_ENTRY {
  TYPE_ID typeIndex;
  STRING* name; // Nullable
  uint8_t elementCount;
  // Not sure why I want this right now
  // Hoping we can do without it.
  // SYMBOL_KIND kind;
} TYPE_FIELD_ENTRY;

typedef struct TYPE_ENTRY {
  TYPE_ID index;
  STRING* module;
  STRING* name;

  TYPE_ENTRY_STATUS status;
  TYPE_ENTRY_TYPE entryType;
  // Pointers and arrays have "parents" (really the pointed type)
  // Function types use fields for parameters, the last field is the return type
  // Records just store fields
  // Unions store the types that overlap
  // Pointers and arrays store their parent type
  TYPE_FIELD_ENTRY* fields;
} TYPE_ENTRY;

TYPE_ENTRY* TYPE_TABLE_init(void);
void TYPE_TABLE_free(void);

TYPE_ID TYPE_declare(STRING* module, STRING* name);
TYPE_ID TYPE_define(TYPE_ID index, TYPE_ENTRY_TYPE entryType, TYPE_FIELD_ENTRY* fields);
TYPE_ID TYPE_registerPrimitive(char* name);

TYPE_ENTRY TYPE_get(TYPE_ID index);
TYPE_ENTRY TYPE_getByName(char* module, char* name);
TYPE_ID TYPE_getIdByName(char* module, char* name);
bool TYPE_hasParent(TYPE_ID index);
TYPE_ID TYPE_getParentId(TYPE_ID index);
TYPE_ENTRY TYPE_getParent(TYPE_ID index);
TYPE_ENTRY_TYPE TYPE_getKind(TYPE_ID type);

/*
   // These belong in the platform layer
bool TYPE_setPrimitiveSize(char* name, int size);
bool TYPE_calculateSizes();
*/

void TYPE_TABLE_report();
void printKind(int kind);
#endif
