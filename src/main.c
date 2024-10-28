#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");
        if (not fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        // todo: evaluate return value
        (void)interpret(line);
    }
}

[[nodiscard]] static char* read_file(char const* const path) {
    auto const file = fopen(path, "rb");
    if (file == nullptr) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    fseek(file, 0L, SEEK_END);
    auto const file_size = (size_t)ftell(file);
    rewind(file);

    auto const buffer = (char*)malloc(file_size + 1);
    if (buffer == nullptr) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    auto const bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

static void run_file(char const* const path) {
    auto const source = read_file(path);
    auto const result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

int main(int const argc, char const* const* const argv) {
    init_vm();

    switch (argc) {
        case 1:
            repl();
            break;
        case 2:
            run_file(argv[1]);
            break;
        default:
            fprintf(stderr, "Usage: clox [path]\n");
            exit(64);
            break;
    }

    free_vm();
}
