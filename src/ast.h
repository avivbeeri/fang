#ifndef ast_h
#define ast_h

typedef struct AST AST; // Forward reference

struct AST {
  enum {
    AST_ERROR,
    AST_NUMBER,
    AST_NEG,
    AST_ADD,
    AST_MUL,
    AST_DIV,
    AST_SUB,
    AST_MOD
  } tag;
  union {
    struct AST_ERROR { int number; } AST_ERROR;
    struct AST_NUMBER { int number; } AST_NUMBER;
    struct AST_NEG { AST *expr; } AST_NEG;
    struct AST_ADD { AST *left; AST *right; } AST_ADD;
    struct AST_MUL { AST *left; AST *right; } AST_MUL;
    struct AST_SUB { AST *left; AST *right; } AST_SUB;
    struct AST_DIV { AST *left; AST *right; } AST_DIV;
    struct AST_MOD { AST *left; AST *right; } AST_MOD;
  } data;
};

AST* ast_new(AST ast);
void ast_free(AST* ptr);
#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}})

#endif
