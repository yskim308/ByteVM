#include "../include/chunk.h"
#include "../include/debug.h"
#include "../include/value.h"
#include <stdio.h>

int main(int argc, char **argv) {
  Chunk chunk;
  init_chunk(&chunk);

  int constant_idx = add_constant(&chunk, 1.2);
  write_chunk(&chunk, OP_CONSTANT, 123);
  write_chunk(&chunk, constant_idx, 123);

  write_chunk(&chunk, OP_RETURN, 123);

  disassemble_chunk(&chunk, "TEST CHUNK");

  free_chunk(&chunk);
}
