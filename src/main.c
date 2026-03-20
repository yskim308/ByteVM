#include "../include/chunk.h"
#include "../include/debug.h"
#include "../include/value.h"
#include <stdio.h>

int main(int argc, char **argv) {
  Chunk chunk;
  init_chunk(&chunk);

  // return test
  write_chunk(&chunk, OP_RETURN);

  int constant_idx = add_constant(&chunk, 1.2);
  write_chunk(&chunk, OP_CONSTANT);
  write_chunk(&chunk, constant_idx);

  disassemble_chunk(&chunk, "TEST CHUNK");

  free_chunk(&chunk);
}
