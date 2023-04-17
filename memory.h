#ifndef memory_h
#define memory_h

#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif
