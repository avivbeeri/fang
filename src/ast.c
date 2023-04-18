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
    case AST_BOOL: {
      break;
    }
    case AST_IDENTIFIER: {
      break;
    }
    case AST_STRING: {
      break;
    }
    case AST_NUMBER: {
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      ast_free(data.value);
      break;
    }
    case AST_UNARY: {
      struct AST_UNARY data = ast.data.AST_UNARY;
      ast_free(data.expr);
      break;
    }
    case AST_BINARY: {
      struct AST_BINARY data = ast.data.AST_BINARY;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
  }
  free(ptr);
}
