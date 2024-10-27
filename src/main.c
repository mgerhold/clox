#include <stdlib.h>
#include "chunk.h"
#include "common.h"
#include "debug.h"

int main() {
    Chunk chunk;
    init_chunk(&chunk);

    write_chunk(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");
    free_chunk(&chunk);
    return EXIT_SUCCESS;
}
