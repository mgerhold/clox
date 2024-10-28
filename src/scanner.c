#include "scanner.h"
#include <stdio.h>
#include <string.h>
#include "common.h"

typedef struct {
    char const* start;
    char const* current;
    int line;
} Scanner;

Scanner scanner;

[[nodiscard]] static bool is_at_end() {
    return *scanner.current == '\0';
}

[[nodiscard]] static Token make_token(TokenType const type) {
    return (Token){
        .type = type,
        .start = scanner.start,
        .length = (int)(scanner.current - scanner.start),
        .line = scanner.line,
    };
}

[[nodiscard]] static Token error_token(char const* const message) {
    return (Token){
        .type = TOKEN_ERROR,
        .start = message,
        .length = (int)strlen(message),
        .line = scanner.line,
    };
}

[[nodiscard]] static char advance() {
    return *(scanner.current++);
}

[[nodiscard]] static bool match(char const expected) {
    if (is_at_end() or *scanner.current != expected) {
        return false;
    }
    ++scanner.current;
    return true;
}

[[nodiscard]] static char peek() {
    return *scanner.current;
}

[[nodiscard]] static char peek_next() {
    if (is_at_end()) {
        return '\0';
    }
    return scanner.current[1];
}

static void skip_whitespace() {
    for (;;) {
        auto const c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                (void)advance();
                break;
            case '\n':
                ++scanner.line;
                (void)advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' and not is_at_end()) {
                        (void)advance();
                    }
                } else {
                    return;
                }
            default:
                return;
        }
    }
}

[[nodiscard]] static Token string() {
    while (peek() != '"' and not is_at_end()) {
        if (peek() == '\n') {
            ++scanner.line;
        }
        (void)advance();
    }
    if (is_at_end()) {
        return error_token("Unterminated string literal.");
    }
    (void)advance();  // Consume closing quote.
    return make_token(TOKEN_STRING);
}

[[nodiscard]] bool is_digit(char const c) {
    return c >= '0' and c <= '9';
}

[[nodiscard]] static Token number() {
    while (is_digit(peek())) {
        (void)advance();
    }

    // Look for a fractional part.
    if (peek() == '.' and is_digit(peek_next())) {
        (void)advance();  // Consume the ".".

        while (is_digit(peek())) {
            (void)advance();
        }
    }
    return make_token(TOKEN_NUMBER);
}

[[nodiscard]] static bool is_alpha(char const c) {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or c == '_';
}

[[nodiscard]] TokenType check_keyword(int const start, int const length, char const* const rest, TokenType const type) {
    if (scanner.current - scanner.start == start + length and memcmp(scanner.start + start, rest, (size_t)length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

[[nodiscard]] static TokenType identifier_type() {
    // clang-format off
    switch (scanner.start[0]) {
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
    // clang-format on
}

[[nodiscard]] static Token identifier() {
    while (is_alpha(peek()) or is_digit(peek())) {
        (void)advance();
    }
    return make_token(identifier_type());
}

void init_scanner(char const* const source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

[[nodiscard]] Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    auto const c = advance();
    if (is_alpha(c)) {
        return identifier();
    }
    if (is_digit(c)) {
        return number();
    }

    // clang-format off
    switch (c) {
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '-': return make_token(TOKEN_MINUS);
        case '+': return make_token(TOKEN_PLUS);
        case '/': return make_token(TOKEN_SLASH);
        case '*': return make_token(TOKEN_STAR);
        case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
    }
    // clang-format on

    return error_token("Unexpected character.");
}
