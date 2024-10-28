#include "debug.h"
#include <stdio.h>
#include "value.h"

void disassemble_chunk(Chunk* const chunk, char const* const name) {
    printf("== %s ==\n", name);
    for (auto offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

[[nodiscard]] static int simple_instruction(char const* name, int offset);

[[nodiscard]] static int constant_instruction(char const* name, Chunk const* chunk, int offset);

[[nodiscard]] int constant_long_instruction(char const* name, Chunk const* chunk, int offset);

int disassemble_instruction(Chunk const* const chunk, int const offset) {
    printf("%04d ", offset);
    if (offset > 0 and chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    auto const instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

[[nodiscard]] static int simple_instruction(char const* const name, int const offset) {
    printf("%s\n", name);
    return offset + 1;
}

[[nodiscard]] static int constant_instruction(char const* const name, Chunk const* const chunk, int const offset) {
    auto const constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

[[nodiscard]] int constant_long_instruction(char const* const name, Chunk const* const chunk, int const offset) {
    auto const constant = (chunk->code[offset + 1] << 16) | (chunk->code[offset + 2] << 8) | chunk->code[offset + 3];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}
