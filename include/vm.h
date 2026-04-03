#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  Byte *ip;
  Value stack[STACK_MAX];
  Value *stack_top;
  Table strings;
  Table globals;
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void init_VM();

void free_VM();

InterpretResult interpret(const char *source);

void push(Value value);

Value pop();

#endif
