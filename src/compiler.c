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

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "ast.h"
#include "memory.h"


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
  PREC_BITWISE,     // << >> ~ ^
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! -
  PREC_REF,         // @ $
  PREC_CALL,        // . ()
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
  AST* variable = AST_NEW(AST_IDENTIFIER, string);
  if (canAssign && match(TOKEN_EQUAL)) {
    AST* expr = expression();
    return AST_NEW(AST_ASSIGNMENT, variable, expr);
  }
  return variable;
}
static AST* string(bool canAssign) {
  // copy the string to memory
  STRING* string = copyString(parser.previous.start + 1, parser.previous.length - 2);
  return AST_NEW(AST_LITERAL, TYPE_STRING, AST_NEW(AST_STRING, string));
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
    case TOKEN_FALSE: return AST_NEW(AST_LITERAL, TYPE_BOOLEAN, AST_NEW(AST_BOOL, false));
    case TOKEN_TRUE: return AST_NEW(AST_LITERAL, TYPE_BOOLEAN, AST_NEW(AST_BOOL, true));
    default: return AST_NEW(AST_ERROR, 0);
  }
}
static AST* number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  return AST_NEW(AST_LITERAL, TYPE_NUMBER, AST_NEW(AST_NUMBER, value));
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
    case TOKEN_MINUS: expr = AST_NEW(AST_UNARY, OP_NEG, operand);
    case TOKEN_BANG: expr = AST_NEW(AST_UNARY, OP_NOT, operand);
    default: expr = AST_NEW(AST_ERROR, 0);
  }
  return expr;
}

static AST* emitAsm() {
  STRING* string = copyString(parser.previous.start + 2, parser.previous.length - 3);
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  return AST_NEW(AST_ASM, string);
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
  return AST_NEW(AST_IDENTIFIER, copyString(parser.previous.start, parser.previous.length));
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
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after function parameter list");
  return params;
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

  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  return decl;
}

static AST* fnDecl() {
  AST* identifier = parseVariable("Expect function identifier");
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function identifier");
  // Parameters
  size_t arity = 0;
  AST* params = NULL;
  if (!check(TOKEN_RIGHT_PAREN)) {
    // params = AST_NEW(AST_LIST, NULL);
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
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameter list");
  }

  consume(TOKEN_COLON,"Expect ':' after function parameter list.");
  AST* returnType = type();
  consume(TOKEN_LEFT_BRACE,"Expect '{' before function body.");
  return AST_NEW(AST_FN, identifier, params, returnType, block());
}


static void beginScope() {

}
static void endScope() {

}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]      = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACKET]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,   PREC_NONE},

  [TOKEN_ASM_CONTENT]     = {NULL,     NULL,   PREC_NONE},

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
  [TOKEN_AT]              = {ref,    NULL,   PREC_REF},
  [TOKEN_DOLLAR]          = {ref,    NULL,   PREC_REF},
  [TOKEN_COLON]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COLON_COLON]     = {NULL,     NULL,   PREC_NONE},

  [TOKEN_EQUAL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]      = {variable, NULL,   PREC_NONE},
  [TOKEN_TYPE_NAME]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]          = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]          = {number,   NULL,   PREC_NONE},
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

static void traverse(AST* ptr, int level) {
  if (ptr == NULL) {
    return;
  }
  AST ast = *ptr;
  //printf("%s\n", getNodeTypeName(ast.tag));
  switch(ast.tag) {
    case AST_ERROR: {
      printf("An error occurred in the tree");
      break;
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      traverse(data.node, level);
      break;
    }
    case AST_WHILE: {
      printf("%*s", level * 2, "");
      struct AST_WHILE data = ast.data.AST_WHILE;
      printf("while (");
      traverse(data.condition, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_FOR: {
      printf("%*s", level * 2, "");
      struct AST_FOR data = ast.data.AST_FOR;
      printf("for (");
      traverse(data.initializer, 0);
      printf("; ");
      traverse(data.condition, 0);
      printf("; ");
      traverse(data.increment, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_IF: {
      printf("%*s", level * 2, "");
      struct AST_IF data = ast.data.AST_IF;
      printf("if (");
      traverse(data.condition, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      if (data.elseClause != NULL) {
        printf("%*s", level * 2, "");
        printf("} else {\n");
        traverse(data.elseClause, level + 1);
      }
      printf("%*s", level * 2, "");
      printf("}\n");
      break;
    }
    case AST_ASSIGNMENT: {
      printf("%*s", level * 2, "");
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      traverse(data.identifier, 0);
      printf(" = ");
      traverse(data.expr, 0);
      break;
    }
    case AST_VAR_INIT: {
      printf("%*s", level * 2, "");
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      printf("var ");
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      printf(" = ");
      traverse(data.expr, 0);
      break;
    }
    case AST_VAR_DECL: {
      printf("%*s", level * 2, "");
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      printf("var ");
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      traverse(data.node, level);
      printf("\n");
      break;
    }
    case AST_TYPE_DECL: {
      printf("%*s", level * 2, "");
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      printf("type ");
      traverse(data.identifier, 0);
      printf("{\n");
      traverse(data.fields, level + 1);
      printf("\n%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_FN: {
      printf("%*s", level * 2, "");
      struct AST_FN data = ast.data.AST_FN;
      printf("fn ");
      traverse(data.identifier, 0);
      printf("(");
      traverse(data.paramList, 0);
      printf("):");
      traverse(data.returnType, 0);
      printf(" ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      traverse(data.identifier, 0);
      printf("(");
      traverse(data.arguments, 0);
      printf(")");
      break;
    }
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      printf("return ");
      if (data.value != NULL) {
        traverse(data.value, 0);
      }
      printf(";");
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      break;
    }
    case AST_PARAM_LIST: {
      AST* next = ptr;
      while (next != NULL) {
        struct AST_PARAM_LIST data = next->data.AST_PARAM_LIST;
        traverse(data.node, 0);
        next = data.next;
        if (next != NULL) {
          printf(", ");
        }
      }
      break;
    }
    case AST_LIST: {
      AST* next = ptr;
      while (next != NULL) {
        struct AST_LIST data = next->data.AST_LIST;
        traverse(data.node, level);
        next = data.next;
        // printf("\n");
      }
      break;
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      printf("------ main --------\n");
      traverse(data.body, level);
      printf("------ complete --------\n");
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      traverse(data.value, 0);
      break;
    }
    case AST_TYPE_NAME: {
      struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
      printf("%s", data.typeName->chars);
      break;
    }
    case AST_ASM: {
      struct AST_ASM data = ast.data.AST_ASM;
      printf("ASM{\n%s}\n", data.code->chars);
      break;
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      printf("%s", data.identifier->chars);
      break;
    }
    case AST_STRING: {
      struct AST_STRING data = ast.data.AST_STRING;
      printf("\"%s\"", data.text->chars);
      break;
    }
    case AST_BOOL: {
      struct AST_BOOL data = ast.data.AST_BOOL;
      printf("%s", data.value ? "true" : "false");
      break;
    }
    case AST_NUMBER: {
      struct AST_NUMBER data = ast.data.AST_NUMBER;
      printf("%i", data.number);
      break;
    }
    case AST_UNARY: {
      struct AST_UNARY data = ast.data.AST_UNARY;
      char* str;
      switch(data.op) {
        case OP_NEG: str = "-"; break;
        case OP_NOT: str = "!"; break;
        case OP_REF: str = "$"; break;
        case OP_DEREF: str = "@"; break;
        default: str = "MISSING";
      }
      printf("%s", str);
      traverse(data.expr, 0);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      char* str = ".";
      traverse(data.left, 0);
      printf("%s", str);
      traverse(data.right, 0);
      break;
    }
    case AST_BINARY: {
      struct AST_BINARY data = ast.data.AST_BINARY;
      char* str;
      switch(data.op) {
        case OP_ADD: str = "+"; break;
        case OP_SUB: str = "-"; break;
        case OP_MUL: str = "*"; break;
        case OP_DIV: str = "/"; break;
        case OP_MOD: str = "%"; break;
        case OP_OR: str = "||"; break;
        case OP_AND: str = "&&"; break;
        case OP_BITWISE_OR: str = "|"; break;
        case OP_BITWISE_AND: str = "&"; break;
        case OP_SHIFT_LEFT: str = "<<"; break;
        case OP_SHIFT_RIGHT: str = ">>"; break;
        case OP_COMPARE_EQUAL: str = "=="; break;
        case OP_NOT_EQUAL: str = "!="; break;
        case OP_GREATER_EQUAL: str = ">="; break;
        case OP_LESS_EQUAL: str = "<="; break;
        case OP_GREATER: str = ">"; break;
        case OP_LESS: str = "<"; break;
        default: str = "MISSING"; break;
      }
      traverse(data.left, 0);
      printf(" %s ", str);
      traverse(data.right, 0);
      break;
    }
    default: {
      printf("\nERROR\n");
      break;
    }
  }
}

static void traverseTree(AST* ptr) {
  traverse(ptr, 1);
  printf("\n");
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
    beginScope();
    expr = block();
    endScope();
  } else if (match(TOKEN_ASM_CONTENT)) {
    expr = emitAsm();
  } else if (match(TOKEN_IF)) {
    expr = ifStatement();
  } else if (match(TOKEN_FOR)) {
    expr = forStatement();
  } else if (match(TOKEN_RETURN)) {
    expr = returnStatement();
  } else if (match(TOKEN_WHILE)) {
    expr = whileStatement();
  } else {
    expr = expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression statement.");
  }
  return AST_NEW(AST_STMT, expr);
}

static AST* typeDecl() {
  AST* identifier = parseVariable("Expect a data type name");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before type definition.");
  AST* fields = fieldList();
  return AST_NEW(AST_TYPE_DECL, identifier, fields);
}

static AST* declaration() {
  AST* decl = NULL;
  if (match(TOKEN_TYPE)) {
    decl = typeDecl();
  } else if (match(TOKEN_FN)) {
    decl = fnDecl();
  } else if (match(TOKEN_VAR)) {
    decl = varDecl();
  } else {
    decl = statement();
  }
  if (parser.panicMode) synchronize();
  return AST_NEW(AST_DECL, decl);
}

void testScanner(const char* source);
bool compile(const char* source) {
  testScanner(source);
  initScanner(source);
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  AST* list = NULL;
  AST* current = NULL;

  while (!check(TOKEN_EOF)) {
    AST* node = AST_NEW(AST_LIST, declaration(), NULL);

    if (list == NULL) {
      list = node;
    } else {
      current->data.AST_LIST.next = node;
    }
    current = node;
  }
  consume(TOKEN_EOF, "Expect end of expression.");
  AST* ast = AST_NEW(AST_MAIN, list);
  if (!parser.hadError) {
    traverseTree(ast);
  }
  ast_free(ast);
  return !parser.hadError;
}

void testScanner(const char* source) {
  initScanner(source);
  advance();

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
