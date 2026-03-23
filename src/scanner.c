#include <stdio.h>
#include <string.h>

#include "../include/common.h"
#include "../include/scanner.h"

typedef struct {
  const char *start;
  const char *current;
  int line;
} Scanner;

Scanner scanner;

void init_scanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 0;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_at_end() { return *scanner.current == '\0'; }

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() { return *scanner.current; }

static char peek_next() {
  if (is_at_end())
    return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (is_at_end())
    return false;
  if (*scanner.current != expected)
    return false;
  scanner.current++;
  return true;
}

static Token make_token(TokenType type) {
  Token token;
  token.type = type;
  token.line = scanner.line;
  token.length = (int)(scanner.current - scanner.start);
  token.start = scanner.start;

  return token;
}

static Token error_token(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skip_whitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    case '\n':
      scanner.line++;
      advance();
      break;
    case '/':
      if (peek_next() == '/') {
        while (peek() != '\n' && !is_at_end())
          advance();
      } else {
        return;
      }
      break;
    deafult:
      return;
    }
  }
}

static Token number() {
  while (is_digit(peek()))
    advance();

  if (peek() == '.' && is_digit(peek_next())) {
    advance();

    while (is_digit(peek()))
      advance();
  }

  return make_token(TOKEN_NUMBER);
}

static Token string() {
  while (peek() != '"' && !is_at_end()) {
    if (peek() == '\n')
      scanner.line++;
    advance();
  }

  if (is_at_end())
    return error_token("Unterminated string");

  advance();
  return make_token(TOKEN_STRING);
}

Token scan_token() {
  skip_whitespace();
  scanner.start = scanner.current;

  if (is_at_end())
    return make_token(TOKEN_EOF);

  char c = advance();
  if (is_digit(c))
    return number();

  switch (c) {
  case '(':
    return make_token(TOKEN_LEFT_PAREN);
  case ')':
    return make_token(TOKEN_RIGHT_PAREN);
  case '{':
    return make_token(TOKEN_LEFT_BRACE);
  case '}':
    return make_token(TOKEN_RIGHT_BRACE);
  case ';':
    return make_token(TOKEN_SEMICOLON);
  case ',':
    return make_token(TOKEN_COMMA);
  case '.':
    return make_token(TOKEN_DOT);
  case '-':
    return make_token(TOKEN_MINUS);
  case '+':
    return make_token(TOKEN_PLUS);
  case '/':
    return make_token(TOKEN_SLASH);
  case '*':
    return make_token(TOKEN_STAR);
  case '!':
    return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"':
    return string();
  }

  return error_token("Unexpected character.");
}
