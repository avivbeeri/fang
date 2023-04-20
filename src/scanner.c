/*
  MIT License

  Copyright (c) 2023 Aviv Beeri
  Copyright (c) 2015 Robert "Bob" Nystrom

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
  const char* start;
  const char* current;
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
          c == '_';
}

static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

static bool isAtEnd() {
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peekNext() {
  if (isAtEnd()) return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (isAtEnd()) return false;
  if (*scanner.current != expected) return false;
  scanner.current++;
  return true;
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token errorToken(const char* message) {
  printf("ERROR ");
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skipWhitespace() {
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
        if (peekNext() == '/') {
          // A comment goes until the end of the line.
          while (peek() != '\n' && !isAtEnd()) advance();
        } else if (peekNext() == '*') {
          // A comment goes until the final */ is found
          uint32_t scopes = 1;
          for (;;) {
            if (isAtEnd()) {
              break;
            }
            if (peek() == '\n') {
              scanner.line++;
            }
            if (peek() == '*' && peekNext() == '/') {
              scopes--;
              if (scopes == 0) {
                break;
              }
            }
            advance();
            if (peek() == '/' && peekNext() == '*') {
              scopes++;
            }
          }
          advance();
          advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

static TokenType checkKeyword(int start, int length,
    const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType checkTypeKeyword() {
  switch (scanner.start[0]) {
    case 'v':
      if (checkKeyword(1, 3, "oid", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
        return TOKEN_TYPE_NAME;
      }
      break;
    case 'p':
      if (checkKeyword(1, 2, "tr", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
        return TOKEN_TYPE_NAME;
      }
      break;
    case 'b':
      if (checkKeyword(1, 3, "ool", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
        return TOKEN_TYPE_NAME;
      }
      break;
    case 'c': return checkKeyword(1, 3, "har", TOKEN_TYPE_NAME);
    case 's':
      if (checkKeyword(1, 5, "tring", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
        return TOKEN_TYPE_NAME;
      }
      break;
    case 'u':
      if (scanner.current - scanner.start > 1) {
        if (checkKeyword(1, 4, "int8", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
          return TOKEN_TYPE_NAME;
        }
        if (checkKeyword(1, 5, "int16", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
          return TOKEN_TYPE_NAME;
        }
      }
      break;
    case 'i':
      if (scanner.current - scanner.start > 1) {
        if (checkKeyword(1, 3, "nt8", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
          return TOKEN_TYPE_NAME;
        }
        if (checkKeyword(1, 4, "nt16", TOKEN_TYPE_NAME) == TOKEN_TYPE_NAME) {
          return TOKEN_TYPE_NAME;
        }
      }
      break;
  }
  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
  switch (scanner.start[0]) {
    case 'c': return checkKeyword(1, 4, "onst", TOKEN_CONST);
    case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    case 'a':
      if (scanner.current - scanner.start > 1) {
        if (checkKeyword(1, 2, "sm", TOKEN_ASM) == TOKEN_ASM) {
          return TOKEN_ASM;
        }
        if (checkKeyword(1, 1, "s", TOKEN_AS) == TOKEN_AS) {
          return TOKEN_AS;
        }
      }
      break;
    case 'v':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 1, "r", TOKEN_VAR);
        }
      }
      break;
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'n': return checkKeyword(2, 0, "", TOKEN_FN);
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
        }
      }
      break;
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'y': return checkKeyword(2, 2, "pe", TOKEN_TYPE);
          case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'e':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'n': return checkKeyword(2, 2, "um", TOKEN_ENUM);
          case 'l': return checkKeyword(2, 2, "se", TOKEN_ELSE);
          case 'x': return checkKeyword(2, 1, "t", TOKEN_EXT);
        }
      }
      break;
    case 'i':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'm': return checkKeyword(2, 4, "port", TOKEN_IMPORT);
          case 'f': return checkKeyword(2, 0, "", TOKEN_IF);
        }
      }
      break;
  }

  TokenType token = checkTypeKeyword();

  return token;
  //return TOKEN_IDENTIFIER;
}


static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
}

static Token number() {
  // TODO: Handle hexadecimal
  while (isDigit(peek())) advance();

  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume the ".".
    advance();

    while (isDigit(peek())) advance();
  }

  return makeToken(TOKEN_NUMBER);
}

static Token character() {
  while (peek() != '\'' && !isAtEnd()) {
    if (peek() == '\\' && peekNext() == '\'') {
      advance();
    }
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated char literal.");

  // The closing quote.
  advance();
  return makeToken(TOKEN_CHAR);
}
static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') scanner.line++;
    if (peek() == '\\' && peekNext() == '"') {
      advance();
    }
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // The closing quote.
  advance();
  return makeToken(TOKEN_STRING);
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);
  char c = advance();
  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case '[': return makeToken(TOKEN_LEFT_BRACKET);
    case ']': return makeToken(TOKEN_RIGHT_BRACKET);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': return makeToken(TOKEN_MINUS);
    case '+': return makeToken(TOKEN_PLUS);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case '^': return makeToken(TOKEN_CARET);
    case '~': return makeToken(TOKEN_TILDE);
    case '@': return makeToken(TOKEN_AT);
    case '$': return makeToken(TOKEN_DOLLAR);
    case '%': return makeToken(TOKEN_PERCENT);
    case '&':
      return makeToken(match('&') ? TOKEN_AND_AND : TOKEN_AND);
    case '|':
      return makeToken(match('|') ? TOKEN_OR_OR : TOKEN_OR);
    case '!':
      return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
      if (match('<')) {
        return makeToken(TOKEN_LESS_LESS);
      }
      return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
      if (match('>')) {
        return makeToken(TOKEN_GREATER_GREATER);
      }
      return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case ':':
      return makeToken(match(':') ? TOKEN_COLON_COLON : TOKEN_COLON);
    case '\'': return character();
    case '"': return string();
  }

  return errorToken("Unexpected character.");
}

const char* getTokenTypeName(TokenType type) {
  switch (type) {
    case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
    case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
    case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
    case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
    case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
    case TOKEN_COMMA: return "COMMA";
    case TOKEN_DOT: return "DOT";
    case TOKEN_AT: return "AT";
    case TOKEN_DOLLAR: return "DOLLAR";
    case TOKEN_MINUS: return "MINUS";
    case TOKEN_PLUS: return "PLUS";
    case TOKEN_STAR: return "STAR";
    case TOKEN_SLASH: return "SLASH";
    case TOKEN_PERCENT: return "PERCENT";
    case TOKEN_SEMICOLON: return "SEMICOLON";
    case TOKEN_CARET: return "CARET";
    case TOKEN_TILDE: return "TILDE";
    case TOKEN_BANG: return "BANG";
    case TOKEN_BANG_EQUAL: return "BANG_EQUAL";
    case TOKEN_EQUAL: return "EQUAL";
    case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
    case TOKEN_GREATER: return "GREATER";
    case TOKEN_GREATER_GREATER: return "GREATER_GREATER";
    case TOKEN_LESS_LESS: return "LESS_LESS";
    case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
    case TOKEN_LESS: return "LESS";
    case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
    case TOKEN_AND: return "AND";
    case TOKEN_AND_AND: return "AND_AND";
    case TOKEN_OR: return "OR";
    case TOKEN_OR_OR: return "OR_OR";
    case TOKEN_COLON: return "COLON";
    case TOKEN_COLON_COLON: return "COLON_COLON";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_TYPE_NAME: return "TYPE_NAME";
    case TOKEN_STRING: return "STRING";
    case TOKEN_NUMBER: return "NUMBER";
    case TOKEN_BOOL: return "BOOL";
    case TOKEN_IMPORT: return "IMPORT";
    case TOKEN_CONST: return "CONST";
    case TOKEN_VAR: return "VAR";
    case TOKEN_EXT: return "EXT";
    case TOKEN_ASM: return "ASM";
    case TOKEN_TYPE: return "TYPE";
    case TOKEN_FN: return "FN";
    case TOKEN_VOID: return "VOID";
    case TOKEN_RETURN: return "RETURN";
    case TOKEN_FALSE: return "FALSE";
    case TOKEN_TRUE: return "TRUE";
    case TOKEN_WHILE: return "WHILE";
    case TOKEN_FOR: return "FOR";
    case TOKEN_IF: return "IF";
    case TOKEN_ELSE: return "ELSE";
    case TOKEN_THIS: return "THIS";
    case TOKEN_ERROR: return "ERROR";
    case TOKEN_ENUM: return "ENUM";
    case TOKEN_EOF: return "EOF";
    case TOKEN_CHAR: return "CHAR";
    case TOKEN_AS: return "AS";
  }
}
