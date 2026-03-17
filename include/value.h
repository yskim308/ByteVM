#ifndef clox_value_h
#define clox_value_h

typedef double Value;

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void init_array(ValueArray *array);

void write_to_value_array(ValueArray *array, Value value);

void free_value_array(ValueArray *array);

#endif
