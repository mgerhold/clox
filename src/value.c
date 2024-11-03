#include "value.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"

void init_value_array(ValueArray* const array) {
    array->values = nullptr;
    array->capacity = 0;
    array->count = 0;
}

void write_value_array(ValueArray* const array, Value const value) {
    if (array->capacity < array->count + 1) {
        auto const old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    ++(array->count);
}

void free_value_array(ValueArray* const array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

void print_value(Value const value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            print_object(value);
            break;
    }
}

[[nodiscard]] bool values_equal(Value const a, Value const b) {
    if (a.type != b.type) {
        return false;
    }
    // clang-format off
    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
        default:         return false; // Unreachable.
    }
    // clang-format on
}
