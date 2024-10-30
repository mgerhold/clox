#pragma once

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,  // constant with 1 byte index
    OP_CONSTANT_LONG,  // constant with 3 byte index
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_RETURN,
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);
[[nodiscard]] int add_constant(Chunk* chunk, Value value);
void write_constant(Chunk* chunk, Value value, int line);
