#include "memory.h"
#include <stdlib.h>
#include "vm.h"

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

static void free_object(Obj* const object) {
    switch (object->type) {
        case OBJ_STRING:
            FREE(ObjString, object);
            break;
        case OBJ_FUNCTION: {
            auto const function = (ObjFunction*)object;
            free_chunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
    }
}

void free_objects() {
    auto object = vm.objects;
    while (object != nullptr) {
        auto const next = object->next;
        free_object(object);
        object = next;
    }
}
