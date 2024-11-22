#pragma once

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalue_count;
    Chunk chunk;
    ObjString const* name;
} ObjFunction;

typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction const* function;
    ObjUpvalue** upvalues;
    int upvalue_count;
} ObjClosure;

[[nodiscard]] ObjClosure* new_closure(ObjFunction const* function);
[[nodiscard]] ObjFunction* new_function();
[[nodiscard]] ObjNative* new_native(NativeFn function);
[[nodiscard]] uint32_t hash_string(char const* chars, int length);
[[nodiscard]] ObjString* reserve_string(int length, uint32_t hash);
[[nodiscard]] ObjString const* copy_string(char const* chars, int length);
[[nodiscard]] ObjUpvalue* new_upvalue(Value* slot);
void print_object(Value value);

static inline bool is_obj_type(Value const value, ObjType const type) {
    return IS_OBJ(value) and AS_OBJ(value)->type == type;
}
