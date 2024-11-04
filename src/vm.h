#pragma once

#include "chunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
    Chunk const* chunk;
    uint8_t const* ip;
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
[[nodiscard]] InterpretResult interpret(const char* source);
void push(Value value);
[[nodiscard]] Value pop();
