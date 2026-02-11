#include "scanner.h"
#include <stdbool.h>
#include <string.h>

Scanner scanner;

static inline bool is_digit(const char c) {
  return c >= '0' && c <= '9';
}

static inline bool is_alpha(const char c) {
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void scanner_init(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

void scanner_free() {
  scanner.start = NULL;
  scanner.current = NULL;
  scanner.line = 1;
}

bool is_at_end() {
  return *scanner.current == '\0';
}

static Token make_token(TokenType type) {
  Token new_tk = {
      type,                             // token type
      scanner.start,                    // starting point of token
      scanner.current - scanner.start,  // token length
      scanner.line                      // line on which token is
  };

  return new_tk;
}

static Token token_error(const char* message) {
  Token tk = {TK_ERROR, message, strlen(message), scanner.line};
  return tk;
}

static char advance() {
  if (!is_at_end())
    return *(scanner.current++);
  return '\0';
}

static bool match(const char c) {
  bool match = (*scanner.current == c);
  if (match && (c != '\0')) {
    scanner.current++;
    if (c == '\n') {
      scanner.line++;
    }
  }
  return match;
}

static inline char peek() {
  return *scanner.current;
}

static char peek_next() {
  if (is_at_end())
    return '\0';
  return scanner.current[1];
}

/* Skip all insignificant characters: tabs, whitespaces, comments, .etc */
static void skip_insig_chars(Token* error) {
  while (true) {
    char c = peek();
    switch (c) {
      case '\n':
        scanner.line++;
        advance();
        break;
      case '\t':
      case ' ':
      case '\r':
        advance();
        break;
      case '/':
        if (peek_next() == '/') {
          // skip single-line comment
          while (!(is_at_end() || match('\n')))
            advance();
        } else if (peek_next() == '*') {
          // skip multiline comment
          bool closed = false;
          while (!is_at_end()) {
            if (peek() == '*' && peek_next() == '/') {
              // consume "*/"
              advance();
              advance();
              closed = true;
              break;
            } else {
              char temp = advance();
              if (temp == '\n')
                scanner.line++;
            }
          }

          if (!closed)
            *error = token_error("Unclosed multiline comment.");
        } else
          return;
        break;
      default:
        return;
    }
  }
}

static Token string() {
  scanner.start = scanner.current;
  while (!(is_at_end() || peek() == '"' || peek() == '\n'))
    advance();
  if (is_at_end() || peek() == '\n')
    return token_error("Unterminated string.");
  Token tk_string = make_token(TK_STRING);
  advance();  // consume the ending '"'
  return tk_string;
}

static Token number() {
  while (is_digit(peek()))
    advance();

  if (peek() == '.' && is_digit(peek_next())) {
    advance();
    while (is_digit(peek()))
      advance();
  }
  return make_token(TK_NUMBER);
}

/* check_keyword: Used to if the current lexeme is a keyword (var, and, or,
 * .etc)
 *
 * Compare the part of the current lexeme starting from @start and the part of
 * the keyword which also starts from @start.
 * */
static TokenType check_keyword(int start,
                               int length,
                               const char* part,
                               TokenType type) {
  // Compare the length of the lexeme and the keyword
  // Compare the lexeme and keyword if they are equal in size
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, part, length) == 0)
    return type;
  return TK_IDENTIFIER;
}

static TokenType identifier_type() {
  switch (scanner.start[0]) {
    case 'i':
      return check_keyword(1, 1, "f", TK_IF);
    case 'v':
      return check_keyword(1, 2, "ar", TK_VAR);
    case 'a':
      return check_keyword(1, 2, "nd", TK_AND);
    case 'o':
      return check_keyword(1, 1, "r", TK_OR);
    case 'c':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l':
            return check_keyword(2, 3, "ass", TK_CLASS);
          case 'o':
            return check_keyword(2, 6, "ntinue", TK_CONTINUE);
          default:
            break;
        }
      }
      break;
    case 'n':
      return check_keyword(1, 2, "il", TK_NIL);
    case 's':
      return check_keyword(1, 4, "uper", TK_SUPER);
    case 'p':
      return check_keyword(1, 4, "rint", TK_PRINT);
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'r':
            return check_keyword(2, 2, "ue", TK_TRUE);
          case 'h':
            return check_keyword(2, 2, "is", TK_THIS);
          default:
            break;
        }
      }
      break;
    case 'r':
      return check_keyword(1, 5, "eturn", TK_RETURN);
    case 'e':
      return check_keyword(1, 3, "lse", TK_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'o':
            return check_keyword(2, 1, "r", TK_FOR);
          case 'u':
            return check_keyword(2, 1, "n", TK_FUN);
          case 'a':
            return check_keyword(2, 3, "lse", TK_FALSE);
          default:
            break;
        }
      }
      break;
    case 'w':
      return check_keyword(1, 4, "hile", TK_WHILE);
    case 'b':
      return check_keyword(1, 4, "reak", TK_BREAK);
  }
  return TK_IDENTIFIER;
}

static Token identifier() {
  while (is_digit(peek()) || is_alpha(peek()))
    advance();

  TokenType type = identifier_type();
  return make_token(type);
}

Token scan_token() {
  // Skip tabs, comments,... and check if there is a multiline comment unclosed
  Token error = {0, NULL, 0, 0};
  skip_insig_chars(&error);

  if (error.type == TK_ERROR)
    return error;

  scanner.start = scanner.current;

  if (is_at_end()) {
    return make_token(TK_EOF);
  }

  char c = advance();
  switch (c) {
    case '(':
      return make_token(TK_LEFT_PAREN);
    case ')':
      return make_token(TK_RIGHT_PAREN);
    case '{':
      return make_token(TK_LEFT_BRACE);
    case '}':
      return make_token(TK_RIGHT_BRACE);
    case ';':
      return make_token(TK_SEMICOLON);
    case ',':
      return make_token(TK_COMMA);
    case '.':
      return make_token(TK_DOT);
    case '+':
      return make_token(TK_PLUS);
    case '-':
      return make_token(TK_MINUS);
    case '*':
      return make_token(TK_STAR);
    case '<':
      return match('=') ? make_token(TK_LESS_EQUAL) : make_token(TK_LESS);
    case '>':
      return match('=') ? make_token(TK_GREATER_EQUAL) : make_token(TK_GREATER);
    case '=':
      return match('=') ? make_token(TK_EQUAL_EQUAL) : make_token(TK_EQUAL);
    case '!':
      return match('=') ? make_token(TK_BANG_EQUAL) : make_token(TK_BANG);
    case '/':
      return make_token(TK_SLASH);
    case '"':
      return string();
    default:
      if (is_digit(c))
        return number();
      else if (is_alpha(c))
        return identifier();
      return token_error("Invalid character.");
  }
}
