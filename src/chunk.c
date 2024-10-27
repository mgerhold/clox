#include "chunk.h"
#include <stdlib.h>
#include "memory.h"

void init_chunk(Chunk* const chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = nullptr;
    chunk->lines = nullptr;
    init_value_array(&chunk->constants);
}

void free_chunk(Chunk* const chunk) {
    free_value_array(&chunk->constants);
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    init_chunk(chunk);
}

void write_chunk(Chunk* const chunk, uint8_t const byte, int const line) {
    if (chunk->capacity < chunk->count + 1) {
        auto const old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    ++(chunk->count);
}

[[nodiscard]] int add_constant(Chunk* const chunk, Value const value) {
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void write_constant(Chunk* const chunk, Value const value, int const line) {
    auto const constant_index = add_constant(chunk, value);
    if (constant_index <= UINT8_MAX) {
        write_chunk(chunk, OP_CONSTANT, line);
        write_chunk(chunk, (uint8_t)constant_index, line);
    } else {
        write_chunk(chunk, OP_CONSTANT_LONG, line);
        write_chunk(chunk, (uint8_t)(constant_index >> 16), line);
        write_chunk(chunk, (uint8_t)(constant_index >> 8), line);
        write_chunk(chunk, (uint8_t)constant_index, line);
    }
}
