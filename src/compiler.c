#include "compiler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
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

static void emit_byte(uint8_t const byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t const byte1, uint8_t const byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
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

static void end_compiler() {
#ifdef DEBUG_PRINT_CODE
    if (not parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
    emit_return();
}

static void expression();
[[nodiscard]] static ParseRule const* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static void binary() {
    auto const operator_type = parser.previous.type;
    auto const rule = get_rule(operator_type);
    parse_precedence((Precedence)(rule->precedence + 1));
    // clang-format off
    switch (operator_type) {
        case TOKEN_PLUS:  emit_byte(OP_ADD);      break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE);   break;
        default: return; // Unreachable.
    }
    // clang-format on
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
    auto const value = strtod(parser.previous.start, nullptr);
    emit_constant(value);
}

static void unary() {
    auto const operator_type = parser.previous.type;

    // Compile the operand.
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    // clang-format off
    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
    // clang-format on
}

static ParseRule const rules[] = {
    [TOKEN_LEFT_PAREN] = { grouping, nullptr, PREC_NONE, },
    [TOKEN_RIGHT_PAREN] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_LEFT_BRACE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_RIGHT_BRACE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_COMMA] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_DOT] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_MINUS] = { unary, binary, PREC_TERM, },
    [TOKEN_PLUS] = { nullptr, binary, PREC_TERM, },
    [TOKEN_SEMICOLON] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_SLASH] = { nullptr, binary, PREC_FACTOR, },
    [TOKEN_STAR] = { nullptr, binary, PREC_FACTOR, },
    [TOKEN_BANG] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_BANG_EQUAL] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_GREATER] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_GREATER_EQUAL] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_LESS] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_LESS_EQUAL] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_IDENTIFIER] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_STRING] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_NUMBER] = { number, nullptr, PREC_NONE, },
    [TOKEN_AND] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_CLASS] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_ELSE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_FALSE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_FOR] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_FUN] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_IF] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_NIL] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_OR] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_PRINT] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_RETURN] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_SUPER] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_THIS] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_TRUE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_VAR] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_WHILE] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_ERROR] = { nullptr, nullptr, PREC_NONE, },
    [TOKEN_EOF] = { nullptr, nullptr, PREC_NONE, },
};

static void parse_precedence(Precedence const precedence) {
    advance();
    auto const prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == nullptr) {
        error("Expect expression.");
        return;
    }

    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();  // consume operator
        auto const infix_rule = get_rule(parser.previous.type)->infix;
        assert(infix_rule != nullptr);
        infix_rule();
    }
}

[[nodiscard]] static ParseRule const* get_rule(TokenType const type) {
    return &rules[type];
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

[[nodiscard]] bool compile(char const* const source, Chunk* const chunk) {
    init_scanner(source);
    compiling_chunk = chunk;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Except end of expression.");
    end_compiler();
    return not parser.had_error;
}
