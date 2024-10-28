#include <stdlib.h>
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main() {
    init_vm();

    Chunk chunk;
    init_chunk(&chunk);

    write_constant(&chunk, 1.2, 1);
    write_constant(&chunk, 3.4, 1);
    write_chunk(&chunk, OP_ADD, 1);
    write_constant(&chunk, 5.6, 1);
    write_chunk(&chunk, OP_DIVIDE, 1);
    write_chunk(&chunk, OP_NEGATE, 1);
    write_chunk(&chunk, OP_RETURN, 1);

    (void)interpret(&chunk);
    free_vm();
    free_chunk(&chunk);
    return EXIT_SUCCESS;
}
