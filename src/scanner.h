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

#ifndef scanner_h
#define scanner_h

#include "compiler.h"

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT, TOKEN_SEMICOLON,
  TOKEN_CARET, TOKEN_TILDE, TOKEN_AT, TOKEN_DOLLAR,
  // One or two character token
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_GREATER_GREATER,
  TOKEN_LESS, TOKEN_LESS_EQUAL,
  TOKEN_LESS_LESS,
  TOKEN_AND, TOKEN_AND_AND,
  TOKEN_OR, TOKEN_OR_OR,

  TOKEN_COLON, TOKEN_COLON_COLON,
  // Literals
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_CHAR, TOKEN_NUMBER,
  TOKEN_TYPE_NAME, TOKEN_BOOL, TOKEN_VOID,

  // Keywords
  TOKEN_IMPORT, TOKEN_CONST, TOKEN_VAR, TOKEN_EXT, TOKEN_AS,
  TOKEN_ASM, TOKEN_TYPE, TOKEN_FN, TOKEN_ENUM, TOKEN_RETURN,
  TOKEN_FALSE, TOKEN_TRUE, TOKEN_WHILE, TOKEN_FOR, TOKEN_DO,
  TOKEN_IF, TOKEN_ELSE, TOKEN_THIS, TOKEN_MODULE,

  // Control
  TOKEN_ERROR, TOKEN_EOF, TOKEN_BEGIN, TOKEN_END
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  const char* fileName;
  int length;
  int line;
  int pos;
} Token;


void initScanner(const SourceFile* sources);
Token scanToken();
const char* getTokenTypeName(TokenType name);
bool SCANNER_addFile(const char* path);


#endif
