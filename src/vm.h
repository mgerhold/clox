#pragma once

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk const* chunk;
    uint8_t const* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void init_vm();
void free_vm();
[[nodiscard]] InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
