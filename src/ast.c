#include <stdlib.h>

#include "ast.h"

AST *ast_new(AST ast) {
  AST *ptr = malloc(sizeof(AST));
  if (ptr) *ptr = ast;
  return ptr;
}

void ast_free(AST *ptr) {
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_ERROR: {
      break;
    }
    case AST_NUMBER: {
      struct AST_NUMBER data = ast.data.AST_NUMBER;
      break;
    }
    case AST_NEG: {
      struct AST_NEG data = ast.data.AST_NEG;
      ast_free(data.expr);
      break;
    }
    case AST_ADD: {
      struct AST_ADD data = ast.data.AST_ADD;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
    case AST_MUL: {
      struct AST_MUL data = ast.data.AST_MUL;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
    case AST_MOD: {
      struct AST_MOD data = ast.data.AST_MOD;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
    case AST_SUB: {
      struct AST_SUB data = ast.data.AST_SUB;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
    case AST_DIV: {
      struct AST_DIV data = ast.data.AST_DIV;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
  }
  free(ptr);
}
