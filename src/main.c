#include "../include/chunk.h"
#include "../include/debug.h"
#include <stdio.h>

int main(int argc, char **argv) {
  Chunk chunk;
  init_chunk(&chunk);
  write_chunk(&chunk, OP_RETURN);
  disassemble_chunk(&chunk, "TEST CHUNK");
  free_chunk(&chunk);
}
