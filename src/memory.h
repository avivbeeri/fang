#ifndef memory_h
#define memory_h

#include "common.h"
#include "data.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))


void* reallocate(void* pointer, size_t oldSize, size_t newSize);
STRING* copyString(const char* chars, size_t length);

#endif
