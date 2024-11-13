#include "compiler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,  // or
    PREC_AND,  // and
    PREC_EQUALITY,  // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,  // + -
    PREC_FACTOR,  // * /
    PREC_UNARY,  // ! -
    PREC_CALL,  // . ()
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} Compiler;

Parser parser;
Compiler* current = nullptr;
Chunk* compiling_chunk;

[[nodiscard]] static Chunk* current_chunk() {
    return compiling_chunk;
}

static void error_at(Token const* const token, char const* const message) {
    if (parser.panic_mode) {
        return;
    }
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(char const* const message) {
    error_at(&parser.previous, message);
}

static void error_at_current(char const* const message) {
    error_at(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        error_at_current(parser.current.start);
    }
}

static void consume(TokenType const type, char const* const message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

[[nodiscard]] static bool check(TokenType const type) {
    return parser.current.type == type;
}

[[nodiscard]] static bool match(TokenType const type) {
    if (not check(type)) {
        return false;
    }
    advance();
    return true;
}

static void emit_byte(uint8_t const byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t const byte1, uint8_t const byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_loop(int const loop_start) {
    emit_byte(OP_LOOP);

    auto const offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emit_byte((uint8_t)((offset >> 8) & 0xFF));
    emit_byte((uint8_t)(offset & 0xFF));
}

static int emit_jump(uint8_t const instruction) {
    emit_byte(instruction);
    emit_byte(0xFF);
    emit_byte(0xFF);
    return current_chunk()->count - 2;
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

[[nodiscard]] static int make_constant(Value const value) {
    auto const constant_index = add_constant(current_chunk(), value);
    return constant_index;
}

static void emit_constant(Value const value) {
    auto const constant_index = make_constant(value);
    if (constant_index <= UINT8_MAX) {
        emit_bytes(OP_CONSTANT, (uint8_t)constant_index);
    } else {
        emit_byte(OP_CONSTANT_LONG);
        uint8_t const bytes[3] = {
            (uint8_t)(constant_index >> 16),
            (uint8_t)(constant_index >> 8),
            (uint8_t)constant_index,
        };
        for (size_t i = 0; i < sizeof(bytes) / sizeof(bytes[0]); ++i) {
            emit_byte(bytes[i]);
        }
    }
}

static void patch_jump(int const offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    auto const jump_distance = current_chunk()->count - offset - 2;

    if (jump_distance > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (uint8_t)((jump_distance >> 8) & 0xFF);
    current_chunk()->code[offset + 1] = (uint8_t)(jump_distance & 0xFF);
}

static void init_compiler(Compiler* const compiler) {
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    current = compiler;
}

static void end_compiler() {
#ifdef DEBUG_PRINT_CODE
    if (not parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
    emit_return();
}

static void begin_scope() {
    ++current->scope_depth;
}

static void end_scope() {
    --current->scope_depth;

    while (current->local_count > 0 and current->locals[current->local_count - 1].depth > current->scope_depth) {
        emit_byte(OP_POP);
        --current->local_count;
    }
}

static void expression();
static void statement();
static void declaration();
[[nodiscard]] static ParseRule const* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static void binary(bool) {
    auto const operator_type = parser.previous.type;
    auto const rule = get_rule(operator_type);
    parse_precedence((Precedence)(rule->precedence + 1));
    // clang-format off
    switch (operator_type) {
        case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT);   break;
        case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL);            break;
        case TOKEN_GREATER:       emit_byte(OP_GREATER);          break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT);    break;
        case TOKEN_LESS:          emit_byte(OP_LESS);             break;
        case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emit_byte(OP_ADD);              break;
        case TOKEN_MINUS:         emit_byte(OP_SUBTRACT);         break;
        case TOKEN_STAR:          emit_byte(OP_MULTIPLY);         break;
        case TOKEN_SLASH:         emit_byte(OP_DIVIDE);           break;
        default: return; // Unreachable.
    }
    // clang-format on
}

static void literal(bool) {
    // clang-format off
    switch (parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL:   emit_byte(OP_NIL);   break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE);  break;
        default: return; // Unreachable.
    }
    // clang-format on
}

static void grouping(bool) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool) {
    auto const value = strtod(parser.previous.start, nullptr);
    emit_constant(NUMBER_VAL(value));
}

static void or_(bool) {
    auto const else_jump = emit_jump(OP_JUMP_IF_FALSE);
    auto const end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void and_(bool) {
    auto const end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    parse_precedence(PREC_AND);
    patch_jump(end_jump);
}

static void string(bool) {
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

[[nodiscard]] static uint8_t identifier_constant(Token const* name);
[[nodiscard]] static int resolve_local(Compiler const* compiler, Token const* name);

static void named_variable(Token const name, bool const can_assign) {
    uint8_t get_op;
    uint8_t set_op;

    auto arg = resolve_local(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    assert(arg >= 0);

    if (can_assign and match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (uint8_t)arg);
    } else {
        emit_bytes(get_op, (uint8_t)arg);
    }
}

static void variable(bool const can_assign) {
    named_variable(parser.previous, can_assign);
}

static void unary(bool) {
    auto const operator_type = parser.previous.type;

    // Compile the operand.
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    // clang-format off
    switch (operator_type) {
        case TOKEN_BANG:  emit_byte(OP_NOT);    break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
    // clang-format on
}

// clang-format off
static ParseRule const rules[] = {
    [TOKEN_LEFT_PAREN]    = { grouping, nullptr, PREC_NONE       },
    [TOKEN_RIGHT_PAREN]   = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_LEFT_BRACE]    = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_RIGHT_BRACE]   = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_COMMA]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_DOT]           = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_MINUS]         = { unary,    binary,  PREC_TERM       },
    [TOKEN_PLUS]          = { nullptr,  binary,  PREC_TERM       },
    [TOKEN_SEMICOLON]     = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_SLASH]         = { nullptr,  binary,  PREC_FACTOR     },
    [TOKEN_STAR]          = { nullptr,  binary,  PREC_FACTOR     },
    [TOKEN_BANG]          = { unary,    nullptr, PREC_NONE       },
    [TOKEN_BANG_EQUAL]    = { nullptr,  binary,  PREC_EQUALITY   },
    [TOKEN_GREATER]       = { nullptr,  binary,  PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL] = { nullptr,  binary,  PREC_COMPARISON },
    [TOKEN_LESS]          = { nullptr,  binary,  PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]    = { nullptr,  binary,  PREC_COMPARISON },
    [TOKEN_EQUAL]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_EQUAL_EQUAL]   = { nullptr,  binary,  PREC_EQUALITY   },
    [TOKEN_IDENTIFIER]    = { variable, nullptr, PREC_NONE       },
    [TOKEN_STRING]        = { string,   nullptr, PREC_NONE       },
    [TOKEN_NUMBER]        = { number,   nullptr, PREC_NONE       },
    [TOKEN_AND]           = { nullptr,  and_,    PREC_AND        },
    [TOKEN_CLASS]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_ELSE]          = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_FALSE]         = { literal,  nullptr, PREC_NONE       },
    [TOKEN_FOR]           = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_FUN]           = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_IF]            = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_NIL]           = { literal,  nullptr, PREC_NONE       },
    [TOKEN_OR]            = { nullptr,  or_,     PREC_NONE       },
    [TOKEN_PRINT]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_RETURN]        = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_SUPER]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_THIS]          = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_TRUE]          = { literal,  nullptr, PREC_NONE       },
    [TOKEN_VAR]           = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_WHILE]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_ERROR]         = { nullptr,  nullptr, PREC_NONE       },
    [TOKEN_EOF]           = { nullptr,  nullptr, PREC_NONE       },
};
// clang-format on

static void parse_precedence(Precedence const precedence) {
    advance();
    auto const prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == nullptr) {
        error("Expect expression.");
        return;
    }

    auto const can_assign = (precedence <= PREC_ASSIGNMENT);
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();  // consume operator
        auto const infix_rule = get_rule(parser.previous.type)->infix;
        assert(infix_rule != nullptr);
        infix_rule(can_assign);

        if (can_assign and match(TOKEN_EQUAL)) {
            error("Invalid assignment target.");
        }
    }
}

[[nodiscard]] static uint8_t identifier_constant(Token const* const name) {
    // Since we allow higher constant indices than 255, this must also be handled here.
    // TODO: Gracefully handle higher indices.
    auto const constant_index = make_constant(OBJ_VAL(copy_string(name->start, name->length)));
    assert(constant_index >= 0 and constant_index <= UINT8_MAX and "Not implemented.");
    return (uint8_t)constant_index;
}

[[nodiscard]] static bool identifiers_equal(Token const* const a, Token const* const b) {
    if (a->length != b->length) {
        return false;
    }
    return memcmp(a->start, b->start, (size_t)a->length) == 0;
}

[[nodiscard]] static int resolve_local(Compiler const* const compiler, Token const* const name) {
    for (auto i = compiler->local_count - 1; i >= 0; --i) {
        auto const local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void add_local(Token const name) {
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    auto const local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;  // Special sentinel value to mark the variable as "uninitialized".
}

static void declare_variable() {
    if (current->scope_depth == 0) {
        // This is a global variable.
        return;
    }

    auto const name = &parser.previous;

    for (auto i = current->local_count - 1; i >= 0; --i) {
        auto const local = &current->locals[i];
        if (local->depth != -1 and local->depth < current->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    add_local(*name);
}

[[nodiscard]] static uint8_t parse_variable(char const* const error_message) {
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current->scope_depth > 0) {
        // For local variables, we don't need to store them in the constants table.
        return 0;
    }

    return identifier_constant(&parser.previous);
}

static void mark_initialized() {
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t const global) {
    if (current->scope_depth > 0) {
        // This is a local variable.
        mark_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, global);
}

[[nodiscard]] static ParseRule const* get_rule(TokenType const type) {
    return &rules[type];
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void block() {
    while (not check(TOKEN_RIGHT_BRACE) and not check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void var_declaration() {
    auto const global = parse_variable("Expect variable name");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void for_statement() {
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    auto loop_start = current_chunk()->count;
    auto exit_jump = -1;
    if (not match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // Condition.
    }

    if (not match(TOKEN_RIGHT_PAREN)) {
        auto const body_jump = emit_jump(OP_JUMP);
        auto const increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP); // Condition.
    }

    end_scope();
}

static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'if'.");

    auto const then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    auto const else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);

    emit_byte(OP_POP);
    if (match(TOKEN_ELSE)) {
        statement();
    }
    patch_jump(else_jump);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void while_statement() {
    auto const loop_start = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    auto const exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                // Do nothing.
                break;
        }
        advance();
    }
}

static void declaration() {
    if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.panic_mode) {
        synchronize();
    }
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}

[[nodiscard]] bool compile(char const* const source, Chunk* const chunk) {
    init_scanner(source);
    Compiler compiler;
    init_compiler(&compiler);
    compiling_chunk = chunk;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();

    while (not match(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_EOF, "Except end of expression.");
    end_compiler();
    return not parser.had_error;
}
