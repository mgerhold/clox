#include "vm.h"
#include <stdio.h>
#include "compiler.h"
#include "debug.h"

VM vm;

static void reset_stack() {
    vm.stack_top = vm.stack;
}

void init_vm() {
    reset_stack();
}

void free_vm() {}

// TODO: Make function static again.
[[nodiscard]] InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        auto const b = pop(); \
        auto const a = pop(); \
        push(a op b); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value const* slot = vm.stack; slot < vm.stack_top; ++slot) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                auto const constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                uint8_t bytes[3];
                bytes[0] = READ_BYTE();
                bytes[1] = READ_BYTE();
                bytes[2] = READ_BYTE();
                auto const constant_index = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
                auto const constant = vm.chunk->constants.values[constant_index];
                push(constant);
                break;
            }
            case OP_NEGATE:
                push(-pop());
                break;
            case OP_ADD:
                BINARY_OP(+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
                break;
            case OP_RETURN: {
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

[[nodiscard]] InterpretResult interpret(char const* const source) {
    compile(source);
    return INTERPRET_OK;
}

void push(Value const value) {
    *(vm.stack_top++) = value;
}

Value pop() {
    return *(--vm.stack_top);
}