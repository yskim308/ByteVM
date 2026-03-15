#include "../include/chunk.h"
#include <stdio.h>

int main(int argc, char **argv) {
  Chunk chunk;
  init_chunk(&chunk);
  write_chunk(&chunk, OP_RETURN);
  free_chunk(&chunk);
}
