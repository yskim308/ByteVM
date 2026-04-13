#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool is_const;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;

  Local locals[UINT16_MAX + 1];
  int local_count;
  int scope_depth;
} Compiler;

Parser parser;

Compiler *current = NULL;

Chunk *compiling_chunk;

Table constants_table;

const int UINT24_MAX = 16777215;

static Chunk *current_chunk() { return &current->function->chunk; }

static void error_at(Token *token, const char *message) {
  if (parser.panic_mode)
    return;
  parser.panic_mode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.had_error = true;
}

static void error(const char *message) { error_at(&parser.previous, message); }

static void error_at_current(const char *message) {
  error_at(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR)
      break;

    error_at_current(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  error_at_current(message);
}

// not really sure the point of this abstraction but okay
static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emit_byte(Byte byte) {
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_two_bytes(Byte byte1, Byte byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_three_bytes(Byte b1, Byte b2, Byte b3) {
  emit_byte(b1);
  emit_byte(b2);
  emit_byte(b3);
}

static void emit_four_bytes(Byte b1, Byte b2, Byte b3, Byte b4) {
  emit_byte(b1);
  emit_byte(b2);
  emit_byte(b3);
  emit_byte(b4);
}

static void emit_global_with_index(Byte op, Byte op_long, int index) {
  if (index <= UINT8_MAX) {
    emit_two_bytes(op, (Byte)index);
  } else if (index <= UINT24_MAX) {
    int lower_byte_mask = 0xff;

    Byte high_byte = (index >> 16) & lower_byte_mask;
    Byte middle_byte = (index >> 8) & lower_byte_mask;
    Byte low_byte = index & lower_byte_mask;

    emit_four_bytes(op_long, high_byte, middle_byte, low_byte);
  } else {
    error("Too many constants in chunk (> 3 bytes).");
  }
}

static void emit_local_with_index(Byte op, Byte op_long, int index) {
  if (index <= UINT8_MAX) {
    emit_two_bytes(op, (Byte)index);
  } else if (index <= UINT16_MAX) {
    int lower_byte_mask = 0xff;

    Byte high_byte = (index >> 8) & lower_byte_mask;
    Byte low_byte = index & lower_byte_mask;

    emit_three_bytes(op_long, high_byte, low_byte);
  } else {
    error("Too many locals (> 2 bytes).");
  }
}

static void emit_loop(int loop_start) {
  emit_byte(OP_LOOP);

  int offset = current_chunk()->count - loop_start + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large");

  emit_byte(offset >> 8 & 0xff);
  emit_byte(offset & 0xff);
}

static int emit_jump(Byte instruction) {
  emit_three_bytes(instruction, 0xff, 0xff);
  return current_chunk()->count - 2;
}

static void patch_jump(int offset) {
  int jump = current_chunk()->count - offset;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  current_chunk()->code[offset] = (jump >> 8) & 0xff;
  current_chunk()->code[offset] = jump & 0xff;
}

static void emit_return() {
  emit_byte(OP_NIL);
  emit_byte(OP_RETURN);
}

static ObjFunction *end_compiler() {
  emit_return();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(current_chunk(), function->name == NULL
                                           ? "<script>"
                                           : function->name->chars);
  }
#endif

  current = current->enclosing;
  return function;
}

static void begin_scope() { current->scope_depth++; }

static void end_scope() {
  current->scope_depth--;
  while (current->local_count > 0 &&
         current->locals[current->local_count - 1].depth >
             current->scope_depth) {
    emit_byte(OP_POP);
    current->local_count--;
  }
}

// forward declarations
static void expression();
static ParseRule *get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static void statement();
static void declaration();
static int identifier_constant(Token *name);
static int resolve_local(Compiler *compiler, Token *name);

static void binary(bool can_assign) {
  TokenType op_type = parser.previous.type;
  ParseRule *rule = get_rule(op_type);
  parse_precedence(rule->precedence + 1);

  switch (op_type) {
  case TOKEN_BANG_EQUAL:
    emit_two_bytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emit_byte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emit_byte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emit_two_bytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emit_byte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emit_two_bytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_PLUS:
    emit_byte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emit_byte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emit_byte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emit_byte(OP_DIVIDE);
    break;
  default:
    return;
  }
}

static Byte argument_list() {
  Byte arg_count = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (arg_count == 255) {
        error("Cannot have more than 255 arguments to function");
      }
      arg_count++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return arg_count;
}

static void call(bool can_assign) {
  Byte arg_count = argument_list();
  emit_two_bytes(OP_CALL, arg_count);
}

static void literal(bool can_assign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emit_byte(OP_FALSE);
    break;
  case TOKEN_TRUE:
    emit_byte(OP_TRUE);
    break;
  case TOKEN_NIL:
    emit_byte(OP_NIL);
    break;
  default:
    return;
  }
}

static void grouping(bool can_assign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static int make_constant(Value value) {
  int constant = add_constant(current_chunk(), value);
  return constant;
}

static void emit_constant(Value value) {
  int const_idx = make_constant(value);
  emit_global_with_index(OP_CONSTANT, OP_CONSTANT_LONG, const_idx);
}

static void init_compiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;

  compiler->local_count = 0;
  compiler->scope_depth = 0;
  compiler->function = new_function();
  current = compiler;
  if (type != TYPE_SCRIPT) {
    current->function->name =
        copy_string(parser.previous.start, parser.previous.length);
  }

  Local *local = &current->locals[current->local_count++];
  local->depth = 0;
  local->is_const = false;
  local->name.start = "";
  local->name.length = 0;
}

static void number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
  emit_constant(OBJ_VAL(
      copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(Token name, bool can_assign) {
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);

  bool is_local = false;
  if (arg == -1) {
    arg = identifier_constant(&name);
  } else {
    is_local = true;
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    if (is_local && current->locals[arg].is_const) {
      error("Cannot reassign const variable");
    }
    expression();
    if (is_local) {
      emit_local_with_index(OP_SET_LOCAL, OP_SET_LOCAL_LONG, arg);
    } else {
      emit_global_with_index(OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, arg);
    }
  } else {
    if (is_local) {
      emit_local_with_index(OP_GET_LOCAL, OP_GET_LOCAL_LONG, arg);
    } else {
      emit_global_with_index(OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, arg);
    }
  }
}

static void variable(bool can_assign) {
  named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign) {
  TokenType op_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (op_type) {
  case TOKEN_BANG:
    emit_byte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emit_byte(OP_NEGATE);
    break;
  default:
    return;
  }
}

static void and_(bool can_assign) {
  int end_jump = emit_jump(OP_JUMP_IF_FALSE);

  emit_byte(OP_POP);
  parse_precedence(PREC_AND);

  patch_jump(end_jump);
}

static void or_(bool can_assign) {
  int to_RHS = emit_jump(OP_JUMP_IF_FALSE);
  int to_end = emit_jump(OP_JUMP);

  patch_jump(to_RHS);
  emit_byte(OP_POP);

  parse_precedence(PREC_OR);
  patch_jump(to_end);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
  advance();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error("Expect expression.");
    return;
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;

  prefix_rule(can_assign);

  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    error("invalid assignment target.");
  }
}

static int identifier_constant(Token *name) {
  ObjString *str = copy_string(name->start, name->length);

  Value value;
  if (table_get(&constants_table, str, &value)) {
    // maybe dangerous?
    return (uint8_t)AS_NUMBER(value);
  }

  uint8_t index = make_constant(OBJ_VAL(str));

  table_set(&constants_table, str, NUMBER_VAL(index));
  return index;
}

static bool identifiers_equal(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler *compiler, Token *name) {
  for (int i = compiler->local_count - 1; i >= 0; --i) {
    Local *local = &compiler->locals[i];
    if (identifiers_equal(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

static void add_local(Token name) {
  if (current->local_count >= UINT8_MAX + 1) {
    error("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
}

static void declare_variable() {
  if (current->scope_depth == 0)
    return;

  Token *name = &parser.previous;
  for (int i = current->local_count - 1; i >= 0; --i) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scope_depth) {
      break;
    }

    if (identifiers_equal(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }
  add_local(*name);
}

static uint8_t parse_variable(const char *error_msg) {
  consume(TOKEN_IDENTIFIER, error_msg);

  declare_variable();
  if (current->scope_depth > 0)
    return 0;

  return identifier_constant(&parser.previous);
}

static void mark_initialized() {
  if (current->scope_depth >= 0)
    return;
  current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t idx) {
  if (current->scope_depth > 0) {
    mark_initialized();
    return;
  }
  emit_two_bytes(OP_DEFINE_GLOBAL, idx);
  emit_global_with_index(OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, idx);
}

static ParseRule *get_rule(TokenType type) { return &rules[type]; }

static void expression() { parse_precedence(PREC_ASSIGNMENT); }

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void function(FunctionType type) {
  Compiler compiler;
  init_compiler(&compiler, type);
  begin_scope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  do {
    current->function->arity++;
    if (current->function->arity > 255) {
      error_at_current("cannot have more than 255 parametesr");
    }
    Byte constant = parse_variable("Expected parameter name.");
    define_variable(constant);
  } while (match(TOKEN_COMMA));
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

  block();

  ObjFunction *function = end_compiler();
  emit_two_bytes(OP_CONSTANT, make_constant(OBJ_VAL(function)));
}

static void fun_declaration() {
  Byte global = parse_variable("Expect function name.");
  mark_initialized();
  function(TYPE_FUNCTION);
  define_variable(global);
}

static void var_declaration(bool is_const) {
  if (is_const && current->scope_depth == 0) {
    error("Cannot declare 'const' in global scope.");
  }

  uint8_t const_idx = parse_variable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    if (is_const) {
      error("'const' variables must be declared with initializer");
    }
    emit_byte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

  define_variable(const_idx);
  if (current->scope_depth > 0) {
    current->locals[current->local_count - 1].is_const = is_const;
  }
}

static void expression_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

static void if_statement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after if.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");

  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();

  int else_jump = emit_jump(OP_JUMP);
  patch_jump(then_jump);
  emit_byte(OP_POP);

  if (match(TOKEN_ELSE))
    statement();
  patch_jump(else_jump);
}

static void print_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value");
  emit_byte(OP_PRINT);
}

static void while_statement() {
  int loop_start = current_chunk()->count;

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");

  int exit_loop = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();
  emit_loop(loop_start);

  patch_jump(exit_loop);
  emit_byte(OP_POP);
}

static void for_statement() {
  begin_scope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // nothing
  } else if (match(TOKEN_VAR)) {
    var_declaration(false);
  } else {
    expression_statement();
  }

  int loop_start = current_chunk()->count;
  int exit_jump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after loop condition.");

    exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int body_jump = emit_jump(OP_JUMP);
    int increment_start = current_chunk()->count;
    expression();
    emit_byte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emit_loop(loop_start);
    loop_start = increment_start;
    patch_jump(body_jump);
  }

  statement();
  emit_loop(loop_start);
  if (exit_jump != -1) {
    patch_jump(exit_jump);
    emit_byte(OP_POP);
  }

  end_scope();
}

static void synchronize() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:; // Do nothing.
    }
    advance();
  }
}

static void declaration() {
  if (match(TOKEN_FUN)) {
    fun_declaration();
  } else if (match(TOKEN_VAR)) {
    var_declaration(false);
  } else if (match(TOKEN_CONST)) {
    var_declaration(true);
  } else {
    statement();
  }
  if (parser.panic_mode) {
    synchronize();
  }
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    print_statement();
  } else if (match(TOKEN_FOR)) {
    for_statement();
  } else if (match(TOKEN_WHILE)) {
    while_statement();
  } else if (match(TOKEN_IF)) {
    if_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else {
    expression_statement();
  }
}

ObjFunction *compile(const char *source) {
  init_scanner(source);
  init_table(&constants_table);

  Compiler compiler;
  init_compiler(&compiler, TYPE_SCRIPT);

  parser.had_error = false;
  parser.panic_mode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_EOF, "Expect end of expression.");
  free_table(&constants_table);

  ObjFunction *function = end_compiler();
  return parser.had_error ? NULL : function;
}
