#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>

typedef struct {
  const char* start;
  const char* current;
  uint16_t line;
} Scanner;

typedef enum {
  // Single-character tokens.
  TK_LEFT_PAREN,
  TK_RIGHT_PAREN,
  TK_LEFT_BRACE,
  TK_RIGHT_BRACE,
  TK_COMMA,
  TK_DOT,
  TK_MINUS,
  TK_PLUS,
  TK_SEMICOLON,
  TK_SLASH,
  TK_STAR,
  // One or two character tokens.
  TK_BANG,
  TK_BANG_EQUAL,
  TK_EQUAL,
  TK_EQUAL_EQUAL,
  TK_GREATER,
  TK_GREATER_EQUAL,
  TK_LESS,
  TK_LESS_EQUAL,
  // Literals.
  TK_IDENTIFIER,
  TK_STRING,
  TK_NUMBER,
  // Keywords.
  TK_AND,
  TK_CLASS,
  TK_ELSE,
  TK_FALSE,
  TK_FOR,
  TK_FUN,
  TK_IF,
  TK_NIL,
  TK_OR,
  TK_PRINT,
  TK_RETURN,
  TK_SUPER,
  TK_THIS,
  TK_TRUE,
  TK_VAR,
  TK_WHILE,

  // Loop control
  TK_BREAK,
  TK_CONTINUE,

  TK_ERROR,
  TK_EOF,
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  uint8_t length;
  uint16_t line;
} Token;

void scanner_init(const char* source);
void scanner_free();
Token scan_token();

#endif
