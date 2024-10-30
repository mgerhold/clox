#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include "compiler.h"
#include "debug.h"

VM vm;

static void reset_stack() {
    vm.stack_top = vm.stack;
}

static void runtime_error(char const* const format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    auto const instruction = (size_t)(vm.ip - vm.chunk->code - 1);
    auto const line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

void init_vm() {
    reset_stack();
}

void free_vm() {}

void push(Value const value) {
    *(vm.stack_top++) = value;
}

[[nodiscard]] Value pop() {
    return *(--vm.stack_top);
}

[[nodiscard]] static Value peek(int const distance) {
    return vm.stack_top[-1 - distance];
}

[[nodiscard]] static bool is_falsey(Value const value) {
    return IS_NIL(value) or (IS_BOOL(value) and not AS_BOOL(value));
}

[[nodiscard]] static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op) \
    do { \
        if (not IS_NUMBER(peek(0)) or not IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        auto const b = AS_NUMBER(pop()); \
        auto const a = AS_NUMBER(pop()); \
        push(value_type(a op b)); \
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
        // clang-format off
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
                if (not IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_NIL:      push(NIL_VAL);                    break;
            case OP_TRUE:     push(BOOL_VAL(true));             break;
            case OP_FALSE:    push(BOOL_VAL(false));            break;
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >);           break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <);           break;
            case OP_ADD:      BINARY_OP(NUMBER_VAL, +);         break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -);         break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *);         break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /);         break;
            case OP_NOT:      push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_EQUAL: {
                auto const b = pop();
                auto const a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_RETURN: {
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
        // clang-format on
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

[[nodiscard]] InterpretResult interpret(char const* const source) {
    Chunk chunk;
    init_chunk(&chunk);
    if (not compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    auto const result = run();
    free_chunk(&chunk);
    return result;
}
