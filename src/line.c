#include "line.h"
#include "memory.h"

void init_line_array(LineArray *array) {
  array->capacity = 0;
  array->count = 0;
  array->entries = NULL;
}

void write_to_line_array(LineArray *array, int line) {
  if (array->count > 0 && array->entries[array->count - 1].line == line) {
    array->entries[array->count - 1].count += 1;
    return;
  }

  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->entries =
        GROW_ARRAY(LineEntry, array->entries, old_capacity, array->capacity);
  }

  LineEntry entry;
  entry.line = line;
  entry.count = 1;

  array->entries[array->count] = entry;
  array->count += 1;
}

void free_line_array(LineArray *array) {
  FREE_ARRAY(LineEntry, array->entries, array->count);
  init_line_array(array);
}

int get_line(LineArray *array, int idx) {
  int processed_lines = 0;
  for (int i = 0; i < array->count; ++i) {
    processed_lines += array->entries[i].count;
    if (processed_lines > idx) {
      return array->entries[i].line;
    }
  }

  return -1;
}
