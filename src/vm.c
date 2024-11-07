#include "vm.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

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
    vm.objects = nullptr;

    init_table(&vm.globals);
    init_table(&vm.strings);
}

void free_vm() {
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

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

static void concatenate() {
    // We first have to create a new string that contains the concatenated contents
    // of the source strings. But if the new string is equal to a string that already
    // has been interned, we will free it immediately. In that case, however, we have
    // to restore the implicit object list to prevent a double free when the VM exits.
    // To be able to do this, we store the head of the objects list as it was before
    // creating a new object.
    auto const objects_list_head = vm.objects;

    auto const b = AS_STRING(pop());
    auto const a = AS_STRING(pop());
    auto const length = a->length + b->length;
    auto const result = reserve_string(length, 0 /* Hash to be filled below. */);
    memcpy(result->chars, a->chars, (size_t)a->length);
    memcpy(result->chars + a->length, b->chars, (size_t)b->length);
    auto const hash = hash_string(result->chars, length);
    result->hash = hash;
    assert(result->chars[length] == '\0' and "reserve_string() shall create a null-terminated string");

    // Check if a string with this content already has been interned.
    auto const interned = table_find_string(&vm.strings, result->chars, length, hash);
    if (interned != nullptr) {
        FREE(ObjString, result);
        // Restore object list.
        vm.objects = objects_list_head;
        push(OBJ_VAL(interned));
    } else {
        // Intern the concatenated string.
        table_set(&vm.strings, result, NIL_VAL);
        push(OBJ_VAL(result));
    }
}

[[nodiscard]] static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
            case OP_POP:      (void)pop();                      break;
            case OP_GET_LOCAL: {
                auto const slot = READ_BYTE();
                push(vm.stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                auto const slot = READ_BYTE();
                vm.stack[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                auto const name = READ_STRING();
                Value value;
                if (not table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                auto const name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                (void)pop();
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >);           break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <);           break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) and IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) and IS_NUMBER(peek(1))) {
                    auto const b = AS_NUMBER(pop());
                    auto const a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -);         break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *);         break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /);         break;
            case OP_NOT:      push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_SET_GLOBAL: {
                auto const name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                auto const b = pop();
                auto const a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_PRINT:
                print_value(pop());
                printf("\n");
                break;
            case OP_RETURN: {
                // Exit interpreter.
                return INTERPRET_OK;
            }
        }
        // clang-format on
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
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
