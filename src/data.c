#include "memory.h"
#include "data.h"

STRING* allocateString(char* chars, int length) {
  STRING* string = reallocate(NULL, 0, sizeof(STRING));
  string->length = length;
  string->chars = chars;
  return string;
}
