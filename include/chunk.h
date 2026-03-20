#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum { OP_RETURN, OP_CONSTANT } OpCOde;

typedef struct {
  int line;
  int count;
} LineEntry;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  LineEntry *lines;
  int line_count;
  ValueArray constants;
} Chunk;

void init_chunk(Chunk *chunk);

void write_chunk(Chunk *chunk, uint8_t byte, int line);

void free_chunk(Chunk *chunk);

int add_constant(Chunk *chunk, Value value);

void init_line_entry(LineEntry *entry, int line);

int get_line(Chunk *chunk, int instruction_idx);

#endif
