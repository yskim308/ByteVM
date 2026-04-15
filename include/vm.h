#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX - 1))

typedef struct {
  ObjClosure *closure;
  Byte *ip;
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[STACK_MAX];
  int frame_count;

  Chunk *chunk;
  Byte *ip;
  Value stack[STACK_MAX];
  Value *stack_top;
  Table strings;
  ObjUpValue *open_upvalues;
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
