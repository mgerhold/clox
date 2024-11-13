#include "object.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) (type*)allocate_object(sizeof(type), object_type)

static Obj* allocate_object(size_t const size, ObjType const type) {
    auto const object = (Obj*)reallocate(nullptr, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

[[nodiscard]] ObjFunction* new_function() {
    auto const function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = nullptr;
    init_chunk(&function->chunk);
    return function;
}

[[nodiscard]] ObjNative* new_native(NativeFn const function) {
    auto const native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

[[nodiscard]] uint32_t hash_string(char const* const chars, int const length) {
    auto hash = 2166136261u;
    for (auto i = 0; i < length; ++i) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619u;
    }
    return hash;
}

[[nodiscard]] ObjString* reserve_string(int const length, uint32_t const hash) {
    auto const string_obj = (ObjString*)reallocate(nullptr, 0, sizeof(ObjString) + (size_t)length + 1);
    string_obj->obj.type = OBJ_STRING;
    string_obj->obj.next = vm.objects;
    vm.objects = (Obj*)string_obj;
    string_obj->chars[length] = '\0';
    string_obj->length = length;
    string_obj->hash = hash;
    return string_obj;
}

[[nodiscard]] ObjString const* copy_string(char const* const chars, int const length) {
    auto const hash = hash_string(chars, length);

    // Check if an equal string already has been interned.
    auto const interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != nullptr) {
        return interned;
    }

    auto const string_obj = reserve_string(length, hash);
    memcpy(string_obj->chars, chars, (size_t)length);

    // Intern string.
    table_set(&vm.strings, string_obj, NIL_VAL);

    return string_obj;
}

static void print_function(ObjFunction const* const function) {
    if (function->name == nullptr) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void print_object(Value const value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
    }
}
