#pragma once

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count) (type*)reallocate(nullptr, 0, sizeof(type) * (size_t)(count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count) \
    (type*)reallocate(pointer, sizeof(type) * (size_t)(old_count), sizeof(type) * (size_t)(new_count))

#define FREE_ARRAY(type, pointer, old_count) reallocate(pointer, sizeof(type) * (size_t)(old_count), 0)

void* reallocate(void* pointer, size_t old_size, size_t new_size);
void free_objects();
