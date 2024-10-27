#include "memory.h"
#include <stdlib.h>

void* reallocate(void* const pointer, size_t, size_t const new_size) {
    if (new_size == 0) {
        free(pointer);
        return nullptr;
    }

    auto const result = realloc(pointer, new_size);
    if (result == nullptr) {
        exit(EXIT_FAILURE);
    }
    return result;
}
