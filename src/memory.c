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

static void strunesc(const char *dest, const char *str, size_t length) {
  char* s = (char*)dest;
  char* t = (char*)str;
  char* next = (char*)str + 1;
  while (length--) {
    if (*t == '\\' && *next == '"') {
      *s = '"';
      length--;
      t += 2;
      next += 2;
    } else {
      *s = *t;
      t++;
      next++;
    }
    s++;
  }
  *s = '\0';
}


STRING* copyString(const char* chars, size_t length) {
  STRING* string = reallocate(NULL, 0, sizeof(STRING));
  string->length = length + 1;
  string->chars = strndup(chars, length);
  strunesc(string->chars, chars, length);
  return string;
}

STRING* createString(const char* chars) {
  return copyString(chars, strlen(chars));
}

void STRING_free(STRING* str) {
  if (str == NULL) {
    return;
  }
  FREE(char, str->chars);
  FREE(STRING, str);
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}
