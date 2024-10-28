#include "compiler.h"
#include <stdio.h>
#include "common.h"
#include "scanner.h"

void compile(char const* const source) {
    init_scanner(source);
    int line = -1;
    for (;;) {
        auto const token = scan_token();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) {
            break;
        }
    }
}
