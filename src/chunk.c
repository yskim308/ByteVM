#include <stdlib.h>

#include "../include/chunk.h"

void init_chunk(Chunk *chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->code = NULL;
}

void write_chunk(Chunk *chunk, uint8_t byte) {
  if (chunk->capacity < chunk->capacity + 1) {
    // grow
  }

  chunk->code[chunk->count] = byte;
  ++chunk->count;
}
