#pragma once

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjFunction const* function;
    uint8_t const* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value* stack_top;
    Table globals;
    Table strings;
    Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void init_vm();
void free_vm();
[[nodiscard]] InterpretResult interpret(char const* source);
void push(Value value);
[[nodiscard]] Value pop();
