#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capcity;
  Entry *entries;
} Table;

void init_table(Table *table);

void free_table(Table *table);

#endif // !CLOX_TABLE_H
