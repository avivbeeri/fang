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
#include <math.h>

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
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // ||
  PREC_AND,         // &&
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_BITWISE,     // << >> ~
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! -
  PREC_REF,         // @ ^
  PREC_CALL,        // . ()
  PREC_SUBSCRIPT,   // []
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

static AST* parseType(bool signature);
static AST* expression();
static AST* declaration();
static AST* statement();
static ParseRule* getRule(TokenType type);
static AST* parsePrecedence(Precedence precedence);

Parser parser;

void errorAt(Token* token, const char* message) {
  if (parser.panicMode) return;

  parser.panicMode = true;
  fprintf(stderr, "[line %d; pos %d] Error", token->line, token->pos);

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
static STRING* parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return copyString(parser.previous.start, parser.previous.length);
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
  STRING* namespace = NULL;
  STRING* string = copyString(parser.previous.start, parser.previous.length);
  if (match(TOKEN_COLON_COLON) && match(TOKEN_IDENTIFIER)) {
    namespace = string;
    string = copyString(parser.previous.start, parser.previous.length);
  }
  AST* variable = AST_NEW_T(AST_IDENTIFIER, parser.previous, namespace, string);
  if (canAssign && match(TOKEN_EQUAL)) {
    Token token = parser.previous;
    AST* expr = expression();
    expr->rvalue = true;
    return AST_NEW_T(AST_ASSIGNMENT, token, variable, expr);
  }
  return variable;
}

static AST* character(bool canAssign) {
  // copy the character to memory
  Value value = CHAR(unesc(parser.previous.start + 1, parser.previous.length - 3));
  int index = CONST_TABLE_store(value);

  return AST_NEW_T(AST_LITERAL, parser.previous, index, value);
}

static AST* string(bool canAssign) {
  // copy the string to memory
  STRING* string = copyString(parser.previous.start + 1, parser.previous.length - 2);
  int index = CONST_TABLE_store(STRING(string));
  return AST_NEW_T(AST_LITERAL, parser.previous, index, PTR(index));
}

static AST* array() {
  AST** values = NULL;
  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      AST* value = parsePrecedence(PREC_OR);
      arrput(values, value);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after a record literal.");
  return AST_NEW(AST_INITIALIZER, values, INIT_TYPE_ARRAY);
}

static AST* subscript(bool canAssign, AST* left) {
  AST* value = expression();
  AST* expr = AST_NEW_T(AST_SUBSCRIPT, parser.previous, left, value);
  consume(TOKEN_RIGHT_BRACKET, "Expect ']' after a subscript.");

  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    right->rvalue = true;
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}

static AST* record() {
  AST** assignments = NULL;
  Token start = parser.previous;
  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      // Becomes an assignment statement because of the coming equality sign
      STRING* name = parseVariable("Expect field value name in record literal.");
      Token paramToken = parser.previous;
      consume(TOKEN_EQUAL, "Expect '=' after field name in record literal.");
      AST* value = NULL;
      if (match(TOKEN_LEFT_BRACE)) {
        value = record();
      } else if (match(TOKEN_LEFT_BRACKET)) {
        value = array();
      } else {
        value = expression();
      }
      if (!match(TOKEN_SEMICOLON) && !match(TOKEN_COMMA) && !check(TOKEN_RIGHT_BRACE)) {
        consume(TOKEN_SEMICOLON, "Expect ';' or ',' after field in record initializer.");
      }
      arrput(assignments, AST_NEW_T(AST_PARAM, paramToken, name, value));
    } while (!check(TOKEN_RIGHT_BRACE));
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after a record literal.");
  return AST_NEW_T(AST_INITIALIZER, start, assignments, INIT_TYPE_RECORD);
}

static AST* literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: return AST_NEW_T(AST_LITERAL, parser.previous, 0, BOOL_VAL(false));
    case TOKEN_TRUE: return AST_NEW_T(AST_LITERAL, parser.previous, 1, BOOL_VAL(true));
    default: return AST_NEW(AST_ERROR, 0);
  }
}

static AST* number(bool canAssign) {
  const char* start = parser.previous.start;
  int32_t value;
  if (start[1] == 'b') {
    // Binary prefix syntax isn't handled by strtol directly, so we advance the string pointer
    value = strtol(start + 2, NULL, 2);
  } else {
    value = strtol(start, NULL, 0);
  }
  int index = CONST_TABLE_store(LIT_NUM(value));
  return AST_NEW_T(AST_LITERAL, parser.previous, index, LIT_NUM(value));
}

static AST* grouping(bool canAssign) {
  AST* expr = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  return expr;
}

static AST* binary(bool canAssign, AST* left) {
  TokenType operatorType = parser.previous.type;
  Token opToken = parser.previous;
  ParseRule* rule = getRule(operatorType);
  AST* right = parsePrecedence((Precedence)(rule->precedence + 1));
  switch (operatorType) {
    case TOKEN_PLUS: return AST_NEW_T(AST_BINARY, opToken, OP_ADD, left, right);
    case TOKEN_MINUS: return AST_NEW_T(AST_BINARY, opToken, OP_SUB, left, right);
    case TOKEN_STAR: return AST_NEW_T(AST_BINARY, opToken, OP_MUL, left, right);
    case TOKEN_SLASH: return AST_NEW_T(AST_BINARY, opToken, OP_DIV, left, right);
    case TOKEN_PERCENT: return AST_NEW_T(AST_BINARY, opToken, OP_MOD, left, right);

    case TOKEN_AND: return AST_NEW_T(AST_BINARY, opToken, OP_BITWISE_AND, left, right);
    case TOKEN_AND_AND: return AST_NEW_T(AST_BINARY, opToken, OP_AND, left, right);
    case TOKEN_OR: return AST_NEW_T(AST_BINARY, opToken, OP_BITWISE_OR, left, right);
    case TOKEN_OR_OR: return AST_NEW_T(AST_BINARY, opToken, OP_OR, left, right);


    case TOKEN_GREATER: return AST_NEW_T(AST_BINARY, opToken, OP_GREATER, left, right);
    case TOKEN_GREATER_GREATER: return AST_NEW_T(AST_BINARY, opToken, OP_SHIFT_RIGHT, left, right);
    case TOKEN_LESS: return AST_NEW_T(AST_BINARY, opToken, OP_LESS, left, right);
    case TOKEN_LESS_LESS: return AST_NEW_T(AST_BINARY, opToken, OP_SHIFT_LEFT, left, right);

    case TOKEN_EQUAL_EQUAL: return AST_NEW_T(AST_BINARY, opToken, OP_COMPARE_EQUAL, left, right);
    case TOKEN_BANG_EQUAL: return AST_NEW_T(AST_BINARY, opToken, OP_NOT_EQUAL, left, right);
    case TOKEN_GREATER_EQUAL: return AST_NEW_T(AST_BINARY, opToken, OP_GREATER_EQUAL, left, right);
    case TOKEN_LESS_EQUAL: return AST_NEW_T(AST_BINARY, opToken, OP_LESS_EQUAL, left, right);
    case TOKEN_CARET: return AST_NEW_T(AST_BINARY, opToken, OP_BITWISE_XOR, left, right);

    default: return AST_NEW_T(AST_ERROR, parser.previous, 0);
  }
}

static AST* ref(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  Token start = parser.previous;
  AST* operand = parsePrecedence(PREC_REF);
  AST* expr = NULL;
  switch (operatorType) {
    case TOKEN_AT: expr = AST_NEW_T(AST_DEREF, start, operand); break;
    case TOKEN_CARET: expr = AST_NEW_T(AST_REF, start, operand); break;
    default: expr = AST_NEW_T(AST_ERROR, start, 0); break;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    right->rvalue = true;
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}
static AST* unary(bool canAssign) {
  Token start = parser.previous;
  TokenType operatorType = parser.previous.type;
  AST* operand = parsePrecedence(PREC_UNARY);
  AST* expr = NULL;
  switch (operatorType) {
    case TOKEN_MINUS: expr = AST_NEW_T(AST_UNARY, start, OP_NEG, operand); break;
    case TOKEN_BANG: expr = AST_NEW_T(AST_UNARY, start, OP_NOT, operand); break;
    case TOKEN_TILDE: expr = AST_NEW_T(AST_UNARY, start, OP_BITWISE_NOT, operand); break;
    default: expr = AST_NEW_T(AST_ERROR, start, 0);
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

static AST* typeFn(bool signature) {
  AST** components = NULL;
  Token start = parser.current;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'fn' in function pointer type declaration.");
  // Function pointer
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      AST* paramType = parseType(true);
      arrput(components, paramType);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after a function pointer type.");
  consume(TOKEN_COLON, "Expect ':' after a function pointer type");
  AST* returnType = parseType(true);
  return AST_NEW_T(AST_TYPE_FN, start, components, returnType);
}

static AST* typePtr(bool signature) {
  Token start = parser.previous;
  AST* subType = parseType(signature);
  return AST_NEW_T(AST_TYPE_PTR, start, subType);
}

static AST* typeArray(bool signature) {
  Token start = parser.previous;
  AST* length = NULL;
  if (!signature) {
    consume(TOKEN_NUMBER, "Expect array size to be a literal when declaring an array type.");
    length = number(false);
  } else if (match(TOKEN_NUMBER)) {
    errorAtCurrent("Array size literal is not allowed in function definitions.");
    return AST_NEW(AST_ERROR, 0);
  }
  consume(TOKEN_RIGHT_BRACKET, "Expect array size literal to be followed by ']'.");
  AST* resultType = parseType(signature);
  return AST_NEW_T(AST_TYPE_ARRAY, start, length, resultType);
}

static AST* parseType(bool signature) {
  if (match(TOKEN_CARET)) {
    return typePtr(signature);
  } else if (match(TOKEN_LEFT_BRACKET)) {
    return typeArray(signature);
  } else if (match(TOKEN_LEFT_PAREN)) {
    AST* subType = parseType(signature);
    consume(TOKEN_RIGHT_PAREN, "Expect matching ')' in type definition.");
    return subType;
  } else if (match(TOKEN_FN)) {
    return typeFn(signature);
  } else if (match(TOKEN_TYPE_NAME) || match(TOKEN_IDENTIFIER)) {
    STRING* module = NULL;
    STRING* name = copyString(parser.previous.start, parser.previous.length);
    if (match(TOKEN_COLON_COLON) && (match(TOKEN_TYPE_NAME) || match(TOKEN_IDENTIFIER))) {
      module = name;
      name = copyString(parser.previous.start, parser.previous.length);
    }
    return AST_NEW_T(AST_TYPE_NAME, parser.previous, module, name);
  } else {
    errorAtCurrent("Expecting a type declaration.");
  }
  return AST_NEW(AST_ERROR, 0);
}

static AST* type(bool signature) {
  Token start = parser.current;
  AST* expr = NULL;

  expr = parseType(signature);
  if (expr == NULL) {
    expr = AST_NEW(AST_ERROR, 0);
  }
  return AST_NEW_T(AST_TYPE, start, expr);
}

static AST** argumentList() {
  AST** arguments = NULL;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      arrput(arguments, expression());
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return arguments;
}

static AST* as(bool canAssign, AST* left) {
  AST* right = type(false);
  AST* expr = AST_NEW_T(AST_CAST, parser.previous, left, right);
  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    right->rvalue = true;
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}
static AST* call(bool canAssign, AST* left) {
  AST** params = argumentList();
  return AST_NEW_T(AST_CALL, parser.previous, left, params);
}


static AST* expression() {
  return parsePrecedence(PREC_ASSIGNMENT);
}

static AST* block() {
  AST** declList = NULL;
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    arrput(declList, declaration());
    if (declList[arrlen(declList)-1]->tag == AST_ERROR) {
      return AST_NEW(AST_ERROR, 0);
    }
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  return AST_NEW(AST_BLOCK, declList);
}


static AST* dot(bool canAssign, AST* left) {
  Token start = parser.previous;
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  STRING* field = copyString(parser.previous.start, parser.previous.length);
  AST* expr = AST_NEW_T(AST_DOT, start, left, field);

  if (canAssign && match(TOKEN_EQUAL)) {
    AST* right = expression();
    right->rvalue = true;
    expr = AST_NEW(AST_ASSIGNMENT, expr, right);
  }
  return expr;
}

/*
static AST* enumValueList() {
  size_t arity = 0;
  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      arity++;
      * identifier = parseVariable("Expect value name");
      if (match(TOKEN_EQUAL)) {
        expression();
      }
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after function parameter list");
  return NULL;
}
*/

static AST** fieldList() {
  AST** params = NULL;
  if (!check(TOKEN_RIGHT_BRACE)) {
    do {
      STRING* identifier = parseVariable("Expect parameter name.");
      consume(TOKEN_COLON, "Expect ':' after parameter name.");
      AST* typeName = type(false);
      consume(TOKEN_SEMICOLON, "Expect ';' after field declaration.");
      AST* param = AST_NEW(AST_PARAM, identifier, typeName);
      arrput(params, param);
    } while (!check(TOKEN_RIGHT_BRACE));
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after function parameter list");
  return params;
}

static AST* constInit() {
  STRING* global = parseVariable("Expect constant name.");
  Token token = parser.previous;
  consume(TOKEN_COLON, "Expect ':' after identifier.");
  AST* varType = type(false);

  consume(TOKEN_EQUAL, "Expect '=' after constant declaration.");
  AST* value;
  if (match(TOKEN_LEFT_BRACE)) {
    value = record();
  } else if (match(TOKEN_LEFT_BRACKET)) {
    value = array();
  } else {
    value = expression();
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  return AST_NEW_T(AST_CONST_DECL, token, global, varType, value);
}

static AST* varInit() {
  STRING* global = parseVariable("Expect variable name");
  Token token = parser.previous;
  consume(TOKEN_COLON, "Expect ':' after identifier.");
  AST* varType = type(false);

  AST* decl = NULL;
  if (match(TOKEN_EQUAL)) {
    AST* value = NULL;
    if (match(TOKEN_LEFT_BRACE)) {
      value = record();
    } else if (match(TOKEN_LEFT_BRACKET)) {
      value = array();
    } else {
      value = expression();
    }
    decl = AST_NEW_T(AST_VAR_INIT, token, global, varType, value);
  } else {
    decl = AST_NEW_T(AST_VAR_DECL, token, global, varType);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  return decl;
}

static AST* fnDecl() {
  STRING* identifier = parseVariable("Expect function name.");
  Token token = parser.previous;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function identifier");

  AST** params = NULL;
  AST** paramTypes = NULL;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      // TODO: Fix a maximum number of parameters here
      STRING* identifier = parseVariable("Expect parameter name.");
      consume(TOKEN_COLON, "Expect ':' after parameter name.");
      AST* typeName = type(true);
      AST* param = AST_NEW(AST_PARAM, identifier, typeName);
      arrput(params, param);
      arrput(paramTypes, typeName);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameter list");
  consume(TOKEN_COLON,"Expect ':' after function parameter list.");
  AST* returnType = type(true);
  consume(TOKEN_LEFT_BRACE,"Expect '{' before function body.");

  AST* fnType = AST_NEW(AST_TYPE_FN, paramTypes, returnType);
  return AST_NEW_T(AST_FN, token, identifier, params, returnType, block(), fnType);
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]      = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACKET]    = {NULL,     subscript, PREC_SUBSCRIPT},
  [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]           = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]            = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]           = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]            = {NULL,     binary, PREC_FACTOR},
  [TOKEN_PERCENT]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]            = {unary,    NULL,   PREC_TERM},
  [TOKEN_TILDE]           = {unary,    NULL,   PREC_TERM},
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
  [TOKEN_CARET]           = {ref,      binary,   PREC_REF},
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
      case TOKEN_ASM:
      case TOKEN_ENUM:
      case TOKEN_EXT:
      case TOKEN_CONST:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_RIGHT_BRACE:
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
static AST* doWhileStatement() {
  consume(TOKEN_WHILE, "Expect 'while' after 'do'");
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  AST* condition = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AST* body = NULL;
  if (!match(TOKEN_SEMICOLON)) {
    body = statement();
  }

  return AST_NEW(AST_DO_WHILE, condition, body);
}
static AST* whileStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  AST* condition = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AST* body = NULL;
  if (!match(TOKEN_SEMICOLON)) {
    body = statement();
  }

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
    initializer = varInit();
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

  AST* body = NULL;
  if (!match(TOKEN_SEMICOLON)) {
    body = statement();
  }

  return AST_NEW(AST_FOR, initializer, condition, increment, body);
}

static AST* returnStatement() {
  AST* expr = NULL;
  if (match(TOKEN_SEMICOLON)) {
    // No return value
  } else {
    expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
  }
  return AST_NEW(AST_RETURN, expr);
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
    expr = returnStatement();
  } else if (match(TOKEN_DO)) {
    expr = doWhileStatement();
  } else if (match(TOKEN_WHILE)) {
    expr = whileStatement();
  } else {
    expr = expressionStatement();
  }
  if (parser.panicMode) synchronize();
  return expr;
}

/*
static AST* enumDecl() {
  AST* identifier = parseVariable("Expect an enum name");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before enum definition.");
  AST* fields = enumValueList();
  return NULL;
}
*/

static AST* typeDecl() {
  STRING* identifier = parseVariable("Expect a data type name");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before type definition.");
  AST** fields = fieldList();
  return AST_NEW(AST_TYPE_DECL, identifier, fields);
}

static AST* importDecl() {
  consume(TOKEN_STRING, "Expect a file path to import");
  STRING* path = copyString(parser.previous.start + 1, parser.previous.length - 2);
  SCANNER_addFile(path->chars);
  // add module namespace to symbol table
  return NULL;
}
static AST* moduleDecl() {
  consume(TOKEN_IDENTIFIER, "Keyword \"module\" should be followed by a module name");
  STRING* name = copyString(parser.previous.start, parser.previous.length);
  return AST_NEW_T(AST_MODULE_DECL, parser.previous, name);
}

static AST* extDecl() {

  SYMBOL_TYPE symbolType = SYMBOL_TYPE_UNKNOWN;
  STRING* identifier;
  AST* dataType = NULL;
  if (match(TOKEN_FN)) {
    identifier = parseVariable("Expect identifier");
    symbolType = SYMBOL_TYPE_FUNCTION;
    AST** params = NULL;
    AST** paramTypes = NULL;
    consume(TOKEN_LEFT_PAREN, "'('");
    if (!check(TOKEN_RIGHT_PAREN)) {
      do {
        // TODO: Fix a maximum number of parameters here
        STRING* identifier = parseVariable("Expect parameter name.");
        consume(TOKEN_COLON, "Expect ':' after parameter name.");
        AST* typeName = type(true);
        AST* param = AST_NEW(AST_PARAM, identifier, typeName);
        arrput(params, param);
        arrput(paramTypes, typeName);
      } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameter list");
    consume(TOKEN_COLON,"Expect ':' after function parameter list.");
    AST* returnType = type(true);
    dataType = AST_NEW(AST_TYPE_FN, paramTypes, returnType);
  } else if (match(TOKEN_CONST)) {
    identifier = parseVariable("Expect identifier");
    consume(TOKEN_COLON, "Expect ':' after parameter name.");
    symbolType = SYMBOL_TYPE_CONSTANT;
    dataType = type(false);
  } else if (match(TOKEN_VAR)) {
    identifier = parseVariable("Expect identifier");
    consume(TOKEN_COLON, "Expect ':' after parameter name.");
    symbolType = SYMBOL_TYPE_VARIABLE;
    dataType = type(false);
  } else {
    return AST_NEW_T(AST_ERROR, parser.previous);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after field in record literal.");
  return AST_NEW_T(AST_EXT, parser.previous, symbolType, identifier, dataType);
}

static STRING* annotation() {
  STRING* str = NULL;
  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect text inside annotation brackets.");
    str = copyString(parser.previous.start, parser.previous.length);
    consume(TOKEN_GREATER, "Expect '>' to conclude an annotation");
  }
  return str;
}

static AST* bank() {
  AST** declList = NULL;
  Token token = parser.previous;
  STRING* str = annotation();
  consume(TOKEN_LEFT_BRACE,"Expect '{' before bank body.");
  while (!check(TOKEN_EOF) && !check(TOKEN_END) && !check(TOKEN_RIGHT_BRACE)) {
    AST* decl = NULL;
    if (match(TOKEN_FN)) {
      decl = fnDecl();
    } else {
      decl = declaration();
    }
    if (decl == NULL) {
      continue;
    }
    if (decl->tag == AST_ERROR) {
      break;
    }
    if (decl) {
      arrput(declList, decl);
    }
  }
  // consume(TOKEN_END, "Expect end of file");
  if (check(TOKEN_EOF) || match(TOKEN_END)) {
  }
  consume(TOKEN_RIGHT_BRACE,"Expect '}' after bank body.");

  return AST_NEW_T(AST_BANK, token, str, declList);
}

static AST* topLevel() {
  AST* decl = NULL;
  if (match(TOKEN_TYPE)) {
    decl = typeDecl();
  } else if (match(TOKEN_BANK)) {
    decl = bank();
  } else if (match(TOKEN_IMPORT)) {
    decl = importDecl();
  } else if (match(TOKEN_EXT)) {
    decl = extDecl();
  } else if (match(TOKEN_ENUM)) {
    //decl = enumDecl();
  } else if (match(TOKEN_FN)) {
    decl = fnDecl();
  } else if (match(TOKEN_VAR)) {
    decl = varInit();
  } else if (match(TOKEN_CONST)) {
    decl = constInit();
  } else {
    decl = AST_NEW(AST_ERROR, 0);
    advance();
    error("Could not find a declaration at the top level.");
  }
  if (parser.panicMode) synchronize();
  return decl;
}

static AST* declaration() {
  AST* decl = NULL;
  Token next = parser.current;
  if (match(TOKEN_VAR)) {
    decl = varInit();
  } else if (match(TOKEN_CONST)) {
    decl = constInit();
  } else if (match(TOKEN_ASM)) {
    decl = asmDecl();
  } else {
    decl = statement();
  }
  if (parser.panicMode) synchronize();
  return decl;
}

static AST* module() {
  AST** declList = NULL;
  if (match(TOKEN_MODULE)) {
    arrput(declList, moduleDecl());
  }
  while (!check(TOKEN_EOF) && !check(TOKEN_END)) {
    AST* decl = topLevel();
    if (decl == NULL) {
      continue;
    }
    if (decl->tag == AST_ERROR) {
      break;
    }
    if (decl) {
      arrput(declList, decl);
    }
  }
  // consume(TOKEN_END, "Expect end of file");
  if (check(TOKEN_EOF) || match(TOKEN_END)) {
  }


  return AST_NEW(AST_MODULE, declList);
}

AST* parse(const char* source) {
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  AST** moduleList = NULL;

  while (!check(TOKEN_EOF)) {
    if (match(TOKEN_BEGIN)) {
      arrput(moduleList, module());
    }
  }

  if (!parser.hadError) {
    consume(TOKEN_EOF, "Expect end of expression.");
  }
  if (parser.hadError) {
    // ast_free(list);
    return NULL;
  }
  return AST_NEW(AST_MAIN, moduleList);
}

void testScanner(const SourceFile* source) {
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
