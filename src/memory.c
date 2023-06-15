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


char* strdup (const char* s)
{
  size_t slen = strlen(s);
  char* result = malloc(slen + 1);
  if(result == NULL)
  {
    return NULL;
  }

  memcpy(result, s, slen+1);
  return result;
}

char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
   if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}


char unesc(const char* str, size_t len) {
  char* t = (char*)str;
  char* next = (char*)str + 1;
  if (*t == '\\') {
    switch (*next) {
      case '0': return '\0';
      case 'n': return '\n';
      case 'r': return '\r';
      case 'a': return '\a';
      case 't': return '\t';
      case 'b': return '\b';
      case 'v': return '\v';
      case 'f': return '\f';
      case '\\': return '\\';
      case '"': return '\"';
      case '\'': return '\'';
      case '?': return '\?';
      case 'x': {
        t += 2;
        len -= 2;
        return strtol(t, NULL, 16);
      };
    }
  }
  return t[0];
}

static size_t strunesc(const char *dest, const char *str, size_t length) {
  char* s = (char*)dest;
  char* t = (char*)str;
  char* next = (char*)str + 1;
  size_t newLength = length;
  while (length--) {
    if (*t == '\\' && (*next == '"' || *next == '\'')) {
      *s = '"';
      length--;
      newLength--;
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
  return newLength;
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

typedef struct {
  char* key;
  size_t length;
} STR_ENTRY;

STR_ENTRY* stringTable = NULL;

STR STR_copy(const char* chars, size_t length) {
  const char* escapedChars = strdup(chars);
  size_t newLength = strunesc(escapedChars, chars, length);
  STR str = shgeti(stringTable, escapedChars);
  if (str == -1) {
    STR_ENTRY entry = ((STR_ENTRY){
      .key = escapedChars,
      .length = newLength
    });
    shputs(stringTable, entry);
    STR str = shgeti(stringTable, escapedChars);
  }
  free(escapedChars);
  return str;
}

STR STR_create(const char* chars) {
  return STR_copy(chars, strlen(chars));
}

STR STR_prepend(STR str, const char* prepend) {
  size_t len = 4;
  size_t i = 0;
  char* buffer = ALLOC_STR(len);
  APPEND_STR(buffer, len, i, "%s%s", prepend, CHARS(str));
  str = STR_copy(buffer, i);
  FREE(char, buffer);
  return str;
}

size_t STR_len(STR str) {
  if (str == -1) {
    return 0;
  }
  return stringTable[str].length;
}
const char* CHARS(STR str) {
  if (str == -1) {
    return NULL;
  }
  return stringTable[str].key;
}

bool STR_compare(STR a, STR b) {
  // Trivial, but for completeness
  return a == b;
}
void STR_init(void) {
  sh_new_arena(stringTable);
}

void STR_free(void) {
  shfree(stringTable);
}
