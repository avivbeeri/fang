#include <stdlib.h>
#include "memory.h"
#include "common.h"

STRING* copyString(const char* chars, size_t length) {
  char* heapChars = strndup(chars, length);
  STRING* string = reallocate(NULL, 0, sizeof(STRING));
  string->length = length + 1;
  string->chars = heapChars;
  return string;
}
void STRING_free(STRING* str) {
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
