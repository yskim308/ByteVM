#ifndef clox_line_h
#define clox_line_h

typedef struct {
  int line;
  int count;
} LineEntry;

typedef struct {
  int capacity;
  int count;
  LineEntry *entries;
} LineArray;

void init_line_array(LineArray *array);

void write_to_line_array(LineArray *array, int line);

void free_line_array(LineArray *array);

int get_line(LineArray *array, int idx);

#endif
