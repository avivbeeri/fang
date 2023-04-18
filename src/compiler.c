#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "ast.h"


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
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef AST* (*ParseFn)();
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static AST* expression();
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

static AST* number() {
  double value = strtod(parser.previous.start, NULL);
  return AST_NEW(AST_NUMBER, value);
}

static AST* grouping() {
  AST* expr = expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  return expr;
}

static AST* binary(AST* left) {
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
    default: return AST_NEW(AST_ERROR, 0);
  }
}

static AST* unary() {
  TokenType operatorType = parser.previous.type;
  AST* operand = parsePrecedence(PREC_UNARY);
  switch (operatorType) {
    case TOKEN_MINUS: return AST_NEW(AST_UNARY, OP_NEG, operand);
    case TOKEN_BANG: return AST_NEW(AST_UNARY, OP_NOT, operand);
    default: return AST_NEW(AST_ERROR, 0);
  }
}

static AST* expression() {
  return parsePrecedence(PREC_ASSIGNMENT);
}


ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]      = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACKET]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COLON]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COLON_COLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]           = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]            = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]           = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]            = {NULL,     binary, PREC_FACTOR},
  [TOKEN_PERCENT]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]            = {unary,    NULL,   PREC_TERM},
  [TOKEN_BANG_EQUAL]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]         = {NULL,     binary, PREC_NONE},
  [TOKEN_GREATER_EQUAL]   = {NULL,     NULL, PREC_NONE},
  [TOKEN_GREATER_GREATER] = {NULL,     binary, PREC_BITWISE},
  [TOKEN_LESS]            = {NULL,     binary, PREC_NONE},
  [TOKEN_LESS_EQUAL]      = {NULL,     NULL, PREC_NONE},
  [TOKEN_LESS_LESS]       = {NULL,     binary, PREC_BITWISE},
  [TOKEN_IDENTIFIER]      = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]          = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]             = {NULL,     binary, PREC_BITWISE},
  [TOKEN_AND_AND]         = {NULL,     binary, PREC_AND},
  [TOKEN_OR]              = {NULL,     binary, PREC_BITWISE},
  [TOKEN_OR_OR]           = {NULL,     binary, PREC_OR},
  [TOKEN_TYPE]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FOR]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FN]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]            = {NULL,     NULL,   PREC_NONE},
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
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return AST_NEW(AST_ERROR, 0);
  }

  AST* expr = prefixRule();
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    expr = infixRule(expr);
  }

  return expr;
}

static void traverse(AST* ptr) {
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR: {
      printf("An error occurred in the tree");
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
        default: str = "ERROR";
      }
      printf("( %s ", str);
      traverse(data.expr);
      printf(" )");
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
        default: str = "ERROR"; break;
      }
      printf("( %s ", str);
      traverse(data.left);
      printf(", ");
      traverse(data.right);
      printf(" )");
      break;
    }
    default: {
      printf("\nERROR\n");
      break;
    }
  }
}

static void traverseTree(AST* ptr) {
  traverse(ptr);
  printf("\n");
}

void testScanner(const char* source);
bool compile(const char* source) {
  initScanner(source);
  parser.hadError = false;
  parser.panicMode = false;

  advance();
  AST* ast = expression();
  consume(TOKEN_EOF, "Expect end of expression.");
  traverseTree(ast);
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
    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOKEN_EOF) break;
  }
}
