#ifndef memory_h
#define memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

typedef struct {
  size_t length;
  char* chars;
} STRING;

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
STRING* copyString(const char* chars, size_t length);
void STRING_free(STRING* str);

#endif
