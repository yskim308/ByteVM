#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"
#include <stdio.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)

#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_NATIVE_OBJ(value) ((ObjNative *)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))

typedef enum {
  OBJ_STRING,
  OBJ_NATIVE,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
} ObjType;

struct Obj {
  ObjType type;
  Obj *next;
};

typedef struct {
  Obj obj;
  int arity;
  int up_value_count;
  Chunk chunk;
  ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int arg_count, Value *args);

typedef struct {
  Obj obj;
  NativeFn function;
  int arity;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  ObjFunction *function
} ObjClosure;

ObjFunction *new_function();
ObjNative *new_native(NativeFn function, int arity);
ObjString *take_string(char *chars, int length);
ObjString *copy_string(const char *chars, int length);
ObjClosure *new_closure(ObjFunction *function);

void print_object(Value value);

static inline bool is_obj_type(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
