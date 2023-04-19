#ifndef ast_h
#define ast_h
#include "common.h"
#include "memory.h"

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
  OP_BITWISE_NOT, // do I need two nots?

  OP_COMPARE_EQUAL,
  OP_NOT_EQUAL,
  OP_GREATER_EQUAL,
  OP_LESS_EQUAL,
} AST_OP;

struct AST {
  enum {
    AST_ERROR,
    AST_LITERAL,
    AST_NUMBER,
    AST_BOOL,
    AST_IDENTIFIER,
    AST_TYPE_NAME,
    AST_STRING,
    AST_UNARY,
    AST_BINARY,
    AST_VAR_DECL,
    AST_VAR_INIT,
    AST_ASSIGNMENT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_DECL,
    AST_STMT,
    AST_FN,
    AST_LIST,
    AST_MAIN,
  } tag;
  union {
    struct AST_ERROR { int number; } AST_ERROR;
    struct AST_NUMBER { int number; } AST_NUMBER;
    struct AST_STRING { STRING* text; } AST_STRING;
    struct AST_IDENTIFIER { STRING* identifier; } AST_IDENTIFIER;
    struct AST_TYPE_NAME { STRING* typeName; } AST_TYPE_NAME;
    struct AST_BOOL { bool value; } AST_BOOL;
    struct AST_LITERAL { AST_TYPE type; AST* value; } AST_LITERAL;
    struct AST_UNARY { AST_OP op; AST *expr; } AST_UNARY;
    struct AST_BINARY { AST_OP op; AST *left; AST *right; } AST_BINARY;
    struct AST_IF { AST* condition; AST* body; AST* elseClause; } AST_IF;
    struct AST_WHILE { AST* condition; AST* body; } AST_WHILE;
    struct AST_FOR { AST* initializer; AST* condition; AST* increment; AST* body; } AST_FOR;
    struct AST_DECL { AST* node; } AST_DECL;
    struct AST_VAR_DECL { AST* identifier; AST* type; } AST_VAR_DECL;
    struct AST_VAR_INIT { AST* identifier; AST* type; AST* expr; } AST_VAR_INIT;
    struct AST_ASSIGNMENT { AST* identifier; AST* expr; } AST_ASSIGNMENT;
    struct AST_FN { AST* identifier; AST* paramList; AST* body; } AST_FN;
    struct AST_STMT { AST* node; } AST_STMT;
    struct AST_LIST { AST* node; AST *next; } AST_LIST;
    struct AST_MAIN { AST* body; } AST_MAIN;
  } data;
};

AST* ast_new(AST ast);
void ast_free(AST* ptr);
#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}})

#endif
