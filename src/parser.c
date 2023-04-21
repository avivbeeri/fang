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

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"
#include "type_table.h"
#include "const_table.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
  bool exitEmit;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // ||
  PREC_AND,         // &&
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_BITWISE,     // << >> ~ ^
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! -
  PREC_REF,         // @ $
  PREC_CALL,        // . ()
  PREC_AS,          // as
  PREC_PRIMARY
} Precedence;

typedef AST* (*ParsePrefixFn)(bool canAssign);
typedef AST* (*ParseInfixFn)(bool canAssign, AST* ast);
typedef struct {
  ParsePrefixFn prefix;
  ParseInfixFn infix;
  Precedence precedence;
} ParseRule;

static AST* expression();
static AST* declaration();
static AST* statement();
static ParseRule* getRule(TokenType type);
static AST* parsePrecedence(Precedence precedence);

Parser parser;

static void errorAt(Token* token, const char* message) {
  if (parser.panicMode) return;

  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static AST* variable(bool canAssign) {
  // copy the string to memory
  STRING* string = copyString(parser.previous.start, parser.previous.length);
  if (canAssign && match(TOKEN_EQUAL)) {
    AST* variable = AST_NEW(AST_LVALUE, string);
    AST* expr = expression();
    return AST_NEW(AST_ASSIGNMENT, variable, expr);
  }
  AST* variable = AST_NEW(AST_IDENTIFIER, string);
  return variable;
}
static AST* character(bool canAssign) {
  // copy the character to memory
  return AST_NEW(AST_LITERAL, CHAR(unesc(parser.previous.start + 1, parser.previous.length - 3)));
  /*
  STRING* string = copyString(parser.previous.start + 1, parser.previous.length - 2);
  int index = CONST_TABLE_store(STRING(string->chars));
  printf("Storing %s at index %i\n", string->chars, index);
  return AST_NEW(AST_LITERAL, STRING(string));
  */
}
static AST* string(bool canAssign) {
  // copy the string to memory
  STRING* string = copyString(parser.previous.start + 1, parser.previous.length - 2);
  int index = CONST_TABLE_store(STRING(string));
  printf("Storing %s at index %i\n", string->chars, index);
  return AST_NEW(AST_LITERAL, STRING(string));
}

static AST* type() {
  if (match(TOKEN_TYPE_NAME)) {
    STRING* string = copyString(parser.previous.start, parser.previous.length);
    return AST_NEW(AST_TYPE_NAME, string);
  } else {
    consume(TOKEN_IDENTIFIER, "Expect a type after identifier");
    STRING* string = copyString(parser.previous.start, parser.previous.length);
    return AST_NEW(AST_IDENTIFIER, string);
  }
}
static AST* literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: return AST_NEW(AST_LITERAL, BOOL_VAL(false));
    case TOKEN_TRUE: return AST_NEW(AST_LITERAL, BOOL_VAL(true));
    default: return AST_NEW(AST_ERROR, 0);
  }
}
static AST* number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  return AST_NEW(AST_LITERAL, NUMBER(value));
}

static AST* grouping(bool canAssign) {
  AST* expr = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  return expr;
}

static AST* binary(bool canAssign, AST* left) {
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  AST* right = parsePrecedence((Precedence)(rule->precedence + 1));
  switch (operatorType) {
    case TOKEN_PLUS: return AST_NEW(AST_BINARY, OP_ADD, left, right);
    case TOKEN_MINUS: return AST_NEW(AST_BINARY, OP_SUB, left, right);
    case TOKEN_STAR: return AST_NEW(AST_BINARY, OP_MUL, left, right);
    case TOKEN_SLASH: return AST_NEW(AST_BINARY, OP_DIV, left, right);
    case TOKEN_PERCENT: return AST_NEW(AST_BINARY, OP_MOD, left, right);

    case TOKEN_AND: return AST_NEW(AST_BINARY, OP_BITWISE_AND, left, right);
    case TOKEN_AND_AND: return AST_NEW(AST_BINARY, OP_AND, left, right);
    case TOKEN_OR: return AST_NEW(AST_BINARY, OP_BITWISE_OR, left, right);
    case TOKEN_OR_OR: return AST_NEW(AST_BINARY, OP_OR, left, right);


    case TOKEN_GREATER: return AST_NEW(AST_BINARY, OP_GREATER, left, right);
    case TOKEN_GREATER_GREATER: return AST_NEW(AST_BINARY, OP_SHIFT_RIGHT, left, right);
    case TOKEN_LESS: return AST_NEW(AST_BINARY, OP_LESS, left, right);
    case TOKEN_LESS_LESS: return AST_NEW(AST_BINARY, OP_SHIFT_LEFT, left, right);

    case TOKEN_EQUAL_EQUAL: return AST_NEW(AST_BINARY, OP_COMPARE_EQUAL, left, right);
    case TOKEN_BANG_EQUAL: return AST_NEW(AST_BINARY, OP_NOT_EQUAL, left, right);
    case TOKEN_GREATER_EQUAL: return AST_NEW(AST_BINARY, OP_GREATER_EQUAL, left, right);
    case TOKEN_LESS_EQUAL: return AST_NEW(AST_BINARY, OP_LESS_EQUAL, left, right);

    default: return AST_NEW(AST_ERROR, 0);
  }
}

static AST* ref(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  AST* operand = parsePrecedence(PREC_REF);
  AST* expr = NULL;
  switch (operatorType) {
    case TOKEN_AT: expr = AST_NEW(AST_UNARY, OP_DEREF, operand); break;
    case TOKEN_DOLLAR: expr = AST_NEW(AST_UNARY, OP_REF, operand); break;
    default: expr = AST_NEW(AST_ERROR, 0); break;
  }
  printf("REF Token: %s\n", getTokenTypeName(operatorType));
  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}
static AST* unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  AST* operand = parsePrecedence(PREC_UNARY);
  AST* expr = NULL;
  switch (operatorType) {
    case TOKEN_MINUS: expr = AST_NEW(AST_UNARY, OP_NEG, operand); break;
    case TOKEN_BANG: expr = AST_NEW(AST_UNARY, OP_NOT, operand); break;
    default: expr = AST_NEW(AST_ERROR, 0);
  }
  return expr;
}

static AST* asmDecl() {
  consume(TOKEN_LEFT_BRACE, "Expect '{' after keyword 'asm'.");
  STRING** output = NULL;
  if (!check(TOKEN_RIGHT_BRACE)) {
    consume(TOKEN_STRING, "ASM blocks can only contain strings.");
    do {
      STRING* string = copyString(parser.previous.start + 1, parser.previous.length - 2);
      arrput(output, string);
    } while (match(TOKEN_STRING));
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after keyword 'asm'.");
  consume(TOKEN_SEMICOLON, "Expect ';' after asm declaration.");
  return AST_NEW(AST_ASM, output);
}

static AST* argumentList(){
  AST* list = NULL;
  if (!check(TOKEN_RIGHT_PAREN)) {
    AST* node = NULL;
    do {
      AST* newNode = AST_NEW(AST_PARAM_LIST, expression(), NULL);
      if (node != NULL) {
        node->data.AST_PARAM_LIST.next = newNode;
      } else {
        list = newNode;
      }
      node = newNode;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return list;
}

static AST* as(bool canAssign, AST* left) {
  AST* right = type();
  AST* expr = AST_NEW(AST_CAST, left, right);
  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}
static AST* call(bool canAssign, AST* left) {
  AST* params = argumentList();
  return AST_NEW(AST_CALL, left, params);
}


static AST* expression() {
  return parsePrecedence(PREC_ASSIGNMENT);
}

static AST* block() {
  AST* list = NULL;
  AST* current = NULL;
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    AST* node = AST_NEW(AST_LIST, declaration(), NULL);
    if (list == NULL) {
      list = node;
    } else {
      current->data.AST_LIST.next = node;
    }
    current = node;
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  return list;
}

static AST* parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return AST_NEW(AST_LVALUE, copyString(parser.previous.start, parser.previous.length));
}

static AST* dot(bool canAssign, AST* left) {
  AST* field = parseVariable("Expect property name after '.'.");
  AST* expr = AST_NEW(AST_DOT, left, field);

  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}

static AST* enumValueList() {
  size_t arity = 0;
  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      arity++;
      AST* identifier = parseVariable("Expect value name");
      if (match(TOKEN_EQUAL)) {
        expression();
      }
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after function parameter list");
  return NULL;
}
static AST* fieldList() {
  size_t arity = 0;
  AST* params = NULL;
  if (!check(TOKEN_RIGHT_BRACE)) {
    AST* node = NULL;
    do {
      arity++;
      AST* identifier = parseVariable("Expect parameter name.");
      consume(TOKEN_COLON, "Expect ':' after parameter name.");
      AST* typeName = type();

      AST* param = AST_NEW(AST_PARAM, identifier, typeName);
      AST* newNode = AST_NEW(AST_PARAM_LIST, param, NULL);
      if (node != NULL) {
        node->data.AST_PARAM_LIST.next = newNode;
      } else {
        params = newNode;
      }
      node = newNode;
      consume(TOKEN_SEMICOLON, "Expect ';' after field declaration.");
    } while (!check(TOKEN_RIGHT_BRACE));
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after function parameter list");
  }
  return params;
}

static AST* constDecl() {
  AST* global = parseVariable("Expect constant name.");
  consume(TOKEN_COLON, "Expect ':' after identifier.");
  AST* varType = type();

  consume(TOKEN_EQUAL, "Expect '=' after constant declaration.");
  AST* value = expression();

  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  return AST_NEW(AST_CONST_DECL, global, varType, value);
}

static AST* varDecl() {
  AST* global = parseVariable("Expect variable name");
  consume(TOKEN_COLON, "Expect ':' after identifier.");
  AST* varType = type();

  AST* decl = NULL;
  if (match(TOKEN_EQUAL)) {
    AST* value = expression();
    decl = AST_NEW(AST_VAR_INIT, global, varType, value);
  } else {
    decl = AST_NEW(AST_VAR_DECL, global, varType);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  return decl;
}

static AST* fnDecl() {
  AST* identifier = parseVariable("Expect function identifier");
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function identifier");
  // Parameters
  size_t arity = 0;
  AST* params = NULL;
  if (!check(TOKEN_RIGHT_PAREN)) {
    AST* node = NULL;
    do {
      arity++;
      // TODO: Fix a maximum number of parameters here
      AST* identifier = parseVariable("Expect parameter name.");
      consume(TOKEN_COLON, "Expect ':' after parameter name.");
      AST* typeName = type();
      AST* param = AST_NEW(AST_PARAM, identifier, typeName);

      AST* newNode = AST_NEW(AST_PARAM_LIST, param, NULL);
      if (node != NULL) {
        node->data.AST_PARAM_LIST.next = newNode;
      } else {
        params = newNode;
      }
      node = newNode;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameter list");
  consume(TOKEN_COLON,"Expect ':' after function parameter list.");
  AST* returnType = type();
  consume(TOKEN_LEFT_BRACE,"Expect '{' before function body.");
  return AST_NEW(AST_FN, identifier, params, returnType, block());
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]      = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACKET]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,   PREC_NONE},

  [TOKEN_MINUS]           = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]            = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]           = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]            = {NULL,     binary, PREC_FACTOR},
  [TOKEN_PERCENT]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]            = {unary,    NULL,   PREC_TERM},
  [TOKEN_BANG_EQUAL]      = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL_EQUAL]     = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL]   = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]            = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]      = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER]         = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_GREATER] = {NULL,     binary, PREC_BITWISE},
  [TOKEN_LESS_LESS]       = {NULL,     binary, PREC_BITWISE},
  [TOKEN_AND]             = {NULL,     binary, PREC_BITWISE},
  [TOKEN_AND_AND]         = {NULL,     binary, PREC_AND},
  [TOKEN_OR]              = {NULL,     binary, PREC_BITWISE},
  [TOKEN_OR_OR]           = {NULL,     binary, PREC_OR},

  [TOKEN_COMMA]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]             = {NULL,     dot,    PREC_CALL},
  [TOKEN_AS]              = {NULL,     as,     PREC_AS},
  [TOKEN_AT]              = {ref,    NULL,   PREC_REF},
  [TOKEN_DOLLAR]          = {ref,    NULL,   PREC_REF},
  [TOKEN_COLON]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COLON_COLON]     = {NULL,     NULL,   PREC_NONE},

  [TOKEN_EQUAL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]      = {variable, NULL,   PREC_NONE},
  [TOKEN_TYPE_NAME]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]          = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]          = {number,   NULL,   PREC_NONE},
  [TOKEN_CHAR]            = {character,NULL,   PREC_NONE},
  [TOKEN_TRUE]            = {literal,  NULL,   PREC_NONE},
  [TOKEN_FALSE]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_TYPE]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FOR]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FN]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_VAR]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EXT]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ASM]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]             = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static AST* parsePrecedence(Precedence precedence) {
  advance();
  ParsePrefixFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return AST_NEW(AST_ERROR, 0);
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  AST* expr = prefixRule(canAssign);
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseInfixFn infixRule = getRule(parser.previous.type)->infix;
    expr = infixRule(canAssign, expr);
  }
  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }

  return expr;
}


static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_TYPE:
      case TOKEN_FN:
      case TOKEN_EXT:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}


static AST* expressionStatement() {
  AST* expr = expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  return expr;
}

static AST* ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  AST* condition = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AST* body = statement();
  AST* elseClause = NULL;
  if (match(TOKEN_ELSE)) {
    elseClause = statement();
  }

  return AST_NEW(AST_IF, condition, body, elseClause);
}
static AST* whileStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  AST* condition = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AST* body = statement();

  return AST_NEW(AST_WHILE, condition, body);
}
static AST* forStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  AST* initializer = NULL;
  AST* condition = NULL;
  AST* increment = NULL;

  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    initializer = varDecl();
  } else {
    initializer = expressionStatement();
  }

  if (!match(TOKEN_SEMICOLON)) {
    condition = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    increment = expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  }

  AST* body = statement();

  return AST_NEW(AST_FOR, initializer, condition, increment, body);
}

static AST* returnStatement(bool topLevel) {
  AST* expr = NULL;
  if (match(TOKEN_SEMICOLON)) {
    // No return value
  } else {
    expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
  }
  parser.exitEmit = parser.exitEmit || topLevel;
  if (topLevel) {
    return AST_NEW(AST_EXIT, expr);
  } else {
    return AST_NEW(AST_RETURN, expr);
  }
}

static AST* statement() {
  AST* expr = NULL;
  if (match(TOKEN_LEFT_BRACE)) {
    expr = block();
  } else if (match(TOKEN_IF)) {
    expr = ifStatement();
  } else if (match(TOKEN_FOR)) {
    expr = forStatement();
  } else if (match(TOKEN_RETURN)) {
    expr = returnStatement(false);
  } else if (match(TOKEN_WHILE)) {
    expr = whileStatement();
  } else {
    expr = expressionStatement();
  }
  return AST_NEW(AST_STMT, expr);
}

static AST* enumDecl() {
  AST* identifier = parseVariable("Expect an enum name");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before enum definition.");
  AST* fields = enumValueList();
  return NULL;
}
static AST* typeDecl() {
  AST* identifier = parseVariable("Expect a data type name");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before type definition.");
  AST* fields = fieldList();
  return AST_NEW(AST_TYPE_DECL, identifier, fields);
}

static AST* topLevel() {
  AST* decl = NULL;
  if (match(TOKEN_TYPE)) {
    decl = typeDecl();
  } else if (match(TOKEN_ENUM)) {
    decl = enumDecl();
  } else if (match(TOKEN_FN)) {
    decl = fnDecl();
  } else if (match(TOKEN_RETURN)) {
    decl = returnStatement(true);
  } else {
    // Go to the other declarations, which are allowed in deeper
    decl = declaration();
  }
  if (parser.panicMode) synchronize();
  return AST_NEW(AST_DECL, decl);
}

static AST* declaration() {
  AST* decl = NULL;
  if (match(TOKEN_VAR)) {
    decl = varDecl();
  } else if (match(TOKEN_CONST)) {
    decl = constDecl();
  } else if (match(TOKEN_ASM)) {
    decl = asmDecl();
  } else {
    decl = statement();
  }
  if (parser.panicMode) synchronize();
  return AST_NEW(AST_DECL, decl);
}

AST* parse(const char* source) {
  parser.hadError = false;
  parser.panicMode = false;
  parser.exitEmit = false;

  advance();
  AST* list = NULL;
  AST* current = NULL;

  while (!check(TOKEN_EOF)) {
    AST* decl = topLevel();
    if (decl == NULL) {
      continue;
    }
    AST* node = AST_NEW(AST_LIST, decl, NULL);

    if (list == NULL) {
      list = node;
    } else {
      current->data.AST_LIST.next = node;
    }
    current = node;
  }

  if (!parser.exitEmit) {
    // Append a "return 0" for exiting safely
    AST* node = AST_NEW(AST_EXIT, AST_NEW(AST_LITERAL, NUMBER(0)));
    if (list == NULL) {
      list = node;
    } else {
      current->data.AST_LIST.next = node;
    }
  }

  consume(TOKEN_EOF, "Expect end of expression.");
  if (parser.hadError) {
    ast_free(list);
    return NULL;
  }
  return AST_NEW(AST_MAIN, list);
}

void testScanner(const char* source) {
  initScanner(source);

  int line = -1;
  for (;;) {
    Token token = scanToken();
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    const char* tokenType = getTokenTypeName(token.type);
    printf("%s '%.*s'\n", tokenType, token.length, token.start);

    if (token.type == TOKEN_EOF) break;
  }
}
