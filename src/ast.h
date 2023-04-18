#ifndef ast_h
#define ast_h
#include "common.h"

typedef struct AST AST; // Forward reference

typedef enum AST_TYPE {
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_BOOLEAN,
  TYPE_RECORD
} AST_TYPE;

typedef enum AST_OP {
  OP_ADD,
  OP_NEG,
  OP_SUB,
  OP_MOD,
  OP_DIV,
  OP_MUL,
  OP_AND,
  OP_OR,
  OP_NOT,
  OP_GREATER,
  OP_LESS,
  OP_SHIFT_LEFT,
  OP_SHIFT_RIGHT,
  OP_BITWISE_AND,
  OP_BITWISE_OR,
  OP_BITWISE_XOR,
  OP_BITWISE_NOT // do I need two nots?
} AST_OP;

struct AST {
  enum {
    AST_ERROR,
    AST_LITERAL,
    AST_NUMBER,
    AST_BOOL,
    AST_IDENTIFIER,
    AST_STRING,
    AST_UNARY,
    AST_BINARY,
  } tag;
  union {
    struct AST_ERROR { int number; } AST_ERROR;
    struct AST_NUMBER { int number; } AST_NUMBER;
    struct AST_STRING { char* text; } AST_STRING;
    struct AST_IDENTIFIER { char* identifier; } AST_IDENTIFIER;
    struct AST_BOOL { bool value; } AST_BOOL;
    struct AST_LITERAL { AST_TYPE type; AST* value; } AST_LITERAL;
    struct AST_UNARY { AST_OP op; AST *expr; } AST_UNARY;
    struct AST_BINARY { AST_OP op; AST *left; AST *right; } AST_BINARY;
  } data;
};

AST* ast_new(AST ast);
void ast_free(AST* ptr);
#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}})

#endif
