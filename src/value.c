#include "value.h"
#include <stdio.h>
#include "memory.h"

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
    printf("%g", value);
}
