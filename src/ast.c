#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "memory.h"

AST *ast_new(AST ast) {
  AST *ptr = reallocate(NULL, 0, sizeof(AST));
  if (ptr) *ptr = ast;
  return ptr;
}

void ast_free(AST *ptr) {
  if (ptr == NULL) {
    return;
  }
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_ERROR: {
      break;
    }
    case AST_BOOL: {
      break;
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      STRING_free(data.identifier);
      break;
    }
    case AST_STRING: {
      struct AST_STRING data = ast.data.AST_STRING;
      STRING_free(data.text);
      break;
    }
    case AST_NUMBER: {
      break;
    }
    case AST_TYPE_NAME: {
      struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
      STRING_free(data.typeName);
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
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      ast_free(data.condition);
      ast_free(data.body);
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      ast_free(data.node);
      break;
    }
    case AST_VAR_DECL: {
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      ast_free(data.identifier);
      ast_free(data.type);
      break;
    }
    case AST_VAR_INIT: {
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      ast_free(data.identifier);
      ast_free(data.type);
      ast_free(data.expr);
      break;
    }
    case AST_ASSIGNMENT: {
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      ast_free(data.identifier);
      ast_free(data.expr);
      break;
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      ast_free(data.node);
      break;
    }
    case AST_FN: {
      struct AST_FN data = ast.data.AST_FN;
      ast_free(data.identifier);
      ast_free(data.paramList);
      ast_free(data.body);
      break;
    }
    case AST_LIST: {
      struct AST_LIST data = ast.data.AST_LIST;
      ast_free(data.node);
      ast_free(data.next);
      break;
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      ast_free(data.body);
      break;
    }
  }
  free(ptr);
}
