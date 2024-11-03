#pragma once

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

[[nodiscard]] uint32_t hash_string(char const* chars, int length);
[[nodiscard]] ObjString* reserve_string(int length, uint32_t hash);
[[nodiscard]] ObjString const* copy_string(char const* chars, int length);
void print_object(Value value);

static inline bool is_obj_type(Value const value, ObjType const type) {
    return IS_OBJ(value) and AS_OBJ(value)->type == type;
}
