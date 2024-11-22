#include "vm.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

VM vm;

static Value clock_native(int, Value*) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value read_number_native(int const args_count, Value* const args) {
    switch (args_count) {
        case 1:
            if (not IS_STRING(args[0])) {
                return NUMBER_VAL(0.0);
            }
            printf("%s", AS_CSTRING(args[0]));
            [[fallthrough]];
        case 0: {
            auto result = 0.0;
            if (scanf("%lf", &result) == 0) {
                return NUMBER_VAL(0.0);
            }
            return NUMBER_VAL(result);
        }
        default:
            // Invalid number of arguments.
            return NUMBER_VAL(0.0);
    }
}

static void reset_stack() {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = nullptr;
}

static void runtime_error(char const* const format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (auto i = vm.frame_count - 1; i >= 0; --i) {
        auto const frame = &vm.frames[i];
        auto const function = frame->closure->function;
        auto const instruction = (size_t)(frame->ip - function->chunk.code - 1);
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == nullptr) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack();
}

static void define_native(char const* const name, NativeFn const function) {
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_native(function)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    (void)pop();
    (void)pop();
}

void init_vm() {
    reset_stack();
    vm.objects = nullptr;

    init_table(&vm.globals);
    init_table(&vm.strings);

    define_native("clock", clock_native);
    define_native("read_number", read_number_native);
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

[[nodiscard]] static bool call(ObjClosure const* const closure, int const arg_count) {
    if (arg_count != closure->function->arity) {
        runtime_error("Expected %d arguments, but got %d.", closure->function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }

    auto const frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

[[nodiscard]] static bool call_value(Value const callee, int const arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE:
                auto const native = AS_NATIVE(callee);
                auto const result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            default:
                break;  // Non-callable object type.
        }
    }
    runtime_error("Can only call functions and classes.");
    return false;
}

[[nodiscard]] static ObjUpvalue* capture_upvalue(Value* const local) {
    auto prev_upvalue = (ObjUpvalue*)nullptr;
    auto upvalue = vm.open_upvalues;
    while (upvalue != nullptr and upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != nullptr and upvalue->location == local) {
        return upvalue;
    }

    auto const created_upvalue = new_upvalue(local);
    created_upvalue->next = upvalue;

    (prev_upvalue == nullptr ? vm.open_upvalues : prev_upvalue->next) = created_upvalue;

    return created_upvalue;
}

static void close_upvalues(Value const* const last) {
    while (vm.open_upvalues != nullptr and vm.open_upvalues->location >= last) {
        auto const upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
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
    auto frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8 | frame->ip[-1])))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
        disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
                auto const constant = frame->closure->function->chunk.constants.values[constant_index];
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
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                auto const slot = READ_BYTE();
                frame->slots[slot] = peek(0);
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
            case OP_GET_UPVALUE: {
                auto const slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                auto const slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
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
            case OP_JUMP: {
                auto const offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                auto const offset = READ_SHORT();
                if (is_falsey(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                auto const offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                auto const arg_count = READ_BYTE();
                if (not call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                auto const function = AS_FUNCTION(READ_CONSTANT());
                auto const closure = new_closure(function);
                push(OBJ_VAL(closure));
                for (auto i = 0; i < closure->upvalue_count; ++i) {
                    auto const is_local = READ_BYTE();
                    auto const index = READ_BYTE();
                    if (is_local == 1) {
                        closure->upvalues[i] = capture_upvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm.stack_top - 1);
                (void)pop();
                break;
            case OP_RETURN: {
                auto const result = pop();
                close_upvalues(frame->slots);
                --vm.frame_count;
                if (vm.frame_count == 0) {
                    (void)pop(); // Pop the main script function.
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
        }
        // clang-format on
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

[[nodiscard]] InterpretResult interpret(char const* const source) {
    auto const function = compile(source);
    if (function == nullptr) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    auto const closure = new_closure(function);
    (void)pop();
    push(OBJ_VAL(closure));
    (void)call(closure, 0);
    return run();
}
