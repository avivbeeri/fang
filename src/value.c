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
#include "value.h"

Value getTypedNumberValue(ValueType type, int32_t n) {
  switch (type) {
    case VAL_BOOL: return U8(n != 0); break;
    case VAL_U8: return U8(n % 256); break;
    case VAL_I8: return I8(n); break;
    case VAL_I16: return I16(n); break;
    case VAL_U16: return U16(n % 32768); break;
    case VAL_PTR: return PTR(n % 32768); break;
    case VAL_LIT_NUM: return LIT_NUM(n); break;
  }
  return ERROR(1);
}

Value getNumericalValue(int32_t n) {
  if (-128 <= n && n <= 127) {
    return I8(n);
  }
  if (n <= 255) {
    return U8(n);
  }
  if (-32768 <= n && n <= 32767) {
    return I16(n);
  }
  if (n <= 32767) {
    return U16(n);
  }
  return LIT_NUM(n);
}

void printValueType(Value value) {
  switch (value.type) {
    case VAL_BOOL: printf("bool"); break;
    case VAL_CHAR: printf("CHAR"); break;
    case VAL_U8: printf("U8"); break;
    case VAL_I8: printf("I8"); break;
    case VAL_I16: printf("I16"); break;
    case VAL_U16: printf("U16"); break;
    case VAL_PTR: printf("PTR"); break;
    case VAL_LIT_NUM: printf("LIT_NUM"); break;
    case VAL_STRING: printf("STRING"); break;
    case VAL_ERROR: printf("ERROR"); break;
    case VAL_RECORD: printf("RECORD"); break;
    case VAL_ARRAY: printf("ARRAY"); break;
    case VAL_UNDEF: printf("0"); break;
  }
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL: printf("%s", AS_BOOL(value) ? "true" : "false"); break;
    case VAL_CHAR: printf("'%c'", AS_CHAR(value)); break;
    case VAL_U8: printf("%hhu", AS_U8(value)); break;
  case VAL_I8: printf("%hhi", AS_I8(value)); break;
    case VAL_I16: printf("%hi", AS_I16(value)); break;
    case VAL_U16: printf("%hu", AS_U16(value)); break;
    case VAL_LIT_NUM: printf("%i", AS_LIT_NUM(value)); break;
    case VAL_PTR: printf("$%hu", AS_PTR(value)); break;
    case VAL_STRING: printf("\"%s\"", AS_STRING(value)->chars); break;
    case VAL_ERROR: printf("ERROR(%i)", AS_ERROR(value)); break;
    case VAL_UNDEF: printf("0"); break;
    case VAL_ARRAY:
      {
        printf("ARRAY{ <print content eventually> }");
        break;
      }
    case VAL_RECORD:
      {
        printf("RECORD{ <print content eventually> }");
        break;
      }
  }
}

int32_t getNumber(Value value) {
  // PRE: Value must be numeric
  switch (value.type) {
    case VAL_BOOL: AS_BOOL(value) ? 1: 0; break;
    case VAL_CHAR: return AS_CHAR(value);
    case VAL_U8: return AS_U8(value);
    case VAL_I8: return AS_I8(value);
    case VAL_I16: return AS_I16(value);
    case VAL_U16: return AS_U16(value);
    case VAL_PTR: return AS_PTR(value);
    case VAL_LIT_NUM: return AS_LIT_NUM(value);
    default: return -1;
  }
  return -1;
}

bool isEqual(Value left, Value right) {
  if (IS_NUMERICAL(left) != IS_NUMERICAL(right)) {
    return false;
  }
  if (IS_NUMERICAL(left)) {
    return getNumber(left) == getNumber(right);
  }
  STRING* leftStr = AS_STRING(left);
  STRING* rightStr = AS_STRING(right);
  if (leftStr->length != rightStr->length) {
    return false;
  }
  size_t len = leftStr->length;
  // TODO: After string interning, this can be changed to the index
  return memcmp(leftStr->chars, rightStr->chars, len);
}

bool isTruthy(Value value) {
  switch (value.type) {
    case VAL_BOOL: return AS_BOOL(value);
    case VAL_CHAR: return AS_CHAR(value) != 0;
    case VAL_U8: return AS_U8(value) != 0;
    case VAL_I8: return AS_I8(value) != 0;
    case VAL_I16: return AS_I16(value) != 0;
    case VAL_U16: return AS_U16(value) != 0;
    case VAL_PTR: return AS_PTR(value) != 0;
    case VAL_LIT_NUM: return AS_LIT_NUM(value) != 0;
    case VAL_STRING: return (AS_STRING(value)->length) > 0;
    case VAL_RECORD: return true;
    case VAL_ARRAY: return true;
    case VAL_UNDEF: return false;
    case VAL_ERROR: return false;
  }
  return false;
}
