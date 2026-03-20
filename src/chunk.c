#include <stdlib.h>

#include "../include/chunk.h"
#include "../include/memory.h"

void init_chunk(Chunk *chunk) {
  chunk->capacity = 0;
  chunk->count = 0;
  chunk->line_count = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  init_value_array(&chunk->constants);
}

void init_line_entry(LineEntry *entry, int line) {
  entry->count = 0;
  entry->line = line;
}

int get_line(Chunk *chunk, int instruction_idx) {
  int lines_covered = 0;
  int lines_idx = 0;

  while (lines_covered < instruction_idx) {
    lines_covered += chunk->lines[lines_idx].count;
    lines_idx += 1;
  }

  return lines_idx - 1;
}

void write_chunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->capacity + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    chunk->lines =
        GROW_ARRAY(LineEntry, chunk->lines, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  if (chunk->lines[chunk->count].line == line) {
    chunk->lines[chunk->count].count += 1;
  } else {
    LineEntry entry;
    init_line_entry(&entry, line);
    chunk->lines[chunk->count + 1] = entry;
  }
  ++chunk->count;
}

void free_chunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  free_value_array(&chunk->constants);
  init_chunk(chunk);
}

int add_constant(Chunk *chunk, Value value) {
  write_to_value_array(&chunk->constants, value);
  return chunk->constants.count - 1;
}
