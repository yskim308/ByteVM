#include "../include/chunk.h"
#include "../include/debug.h"
#include "../include/vm.h"

#include <stdio.h>

int main(int argc, char **argv) {
  init_VM();

  Chunk chunk;
  init_chunk(&chunk);

  int constant_idx = add_constant(&chunk, 1.2);
  write_chunk(&chunk, OP_CONSTANT, 123);
  write_chunk(&chunk, constant_idx, 123);
  write_chunk(&chunk, OP_NEGATE, 123);

  write_chunk(&chunk, OP_RETURN, 123);

  disassemble_chunk(&chunk, "TEST CHUNK");
  interpret(&chunk);

  free_VM();
  free_chunk(&chunk);
}
