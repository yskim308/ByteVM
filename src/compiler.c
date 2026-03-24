#include <cstdlib>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/common.h"
#include "../include/compiler.h"
#include "../include/scanner.h"

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

Parser parser;

Chunk *compiling_chunk;

static Chunk *current_chunk() { return compiling_chunk; }

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
    fprintf(stderr, "at '%.*s'", token->length, token->start);
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

static void emit_byte(Byte byte) {
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_two_bytes(Byte byte1, Byte byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_return() { emit_byte(OP_RETURN); }

static void end_compiler() { emit_return(); }

static Byte make_constant(Value value) {
  int constant = add_constant(current_chunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk");
    return 0;
  }

  return (Byte)constant;
}

static void emit_constant(Value value) {
  emit_two_bytes(OP_CONSTANT, make_constant(value));
}

static void number() {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(value);
}

bool compile(const char *source, Chunk *chunk) {
  init_scanner(source);

  compiling_chunk = chunk;

  parser.had_error = false;
  parser.panic_mode = false;

  advance();
  expression();

  consume(TOKEN_EOF, "Expect end of expression.");
  end_compiler();
  return !parser.had_error;
}
