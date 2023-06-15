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

#ifndef memory_h
#define memory_h

#include <stdio.h>
#include <math.h>
#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define ALLOC_STR(len) reallocate(NULL, 0, len * sizeof(char));
#define APPEND_STR(buffer, len, i, format, ...) do { \
  size_t writeLen = 0; \
  do { \
    if (writeLen >= (len - i)) { \
      size_t oldLen = len; \
      len *= 2; \
      buffer = reallocate(buffer, oldLen, len * sizeof(char)); \
    } \
    writeLen = snprintf(buffer + i, fmax(len - i, 0), format, ##__VA_ARGS__); \
    if (writeLen < 0) { \
      exit(1); \
    } \
  } while (writeLen >= (len - i)); \
  i += writeLen; \
} while(0);

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
char unesc(const char* str, size_t length);
char* readFile(const char* path);

// New interface
typedef size_t STR;
#define EMPTY_STRING -1
STR STR_create(const char* chars);
STR STR_copy(const char* chars, size_t length);
STR STR_prepend(STR str, const char* prepend);
size_t STR_len(STR str);
const char* CHARS(STR str);
bool STR_compare(STR a, STR b); // Trivial, but for completeness
void STR_init(void);
void STR_free(void);


#endif
