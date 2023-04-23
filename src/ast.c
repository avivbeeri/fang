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

#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "memory.h"

static uint64_t id = 0;

AST *ast_new(AST ast) {
  AST *ptr = reallocate(NULL, 0, sizeof(AST));
  if (ptr) *ptr = ast;
  ptr->id = id;
  id++;
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
    case AST_ASM: {
      struct AST_ASM data = ast.data.AST_ASM;
      for (int i = 0; i < arrlen(data.strings); i++) {
        STRING_free(data.strings[i]);
      }
      arrfree(data.strings);
      break;
    }
    case AST_LVALUE: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      STRING_free(data.identifier);
      break;
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      STRING_free(data.identifier);
      break;
    }
    case AST_TYPE_NAME: {
      struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
      STRING_free(data.typeName);
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
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
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      ast_free(data.left);
      ast_free(data.right);
      break;
    }
    case AST_WHILE: {
      struct AST_WHILE data = ast.data.AST_WHILE;
      ast_free(data.condition);
      ast_free(data.body);
      break;
    }
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      ast_free(data.initializer);
      ast_free(data.condition);
      ast_free(data.increment);
      ast_free(data.body);
      break;
    }
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      ast_free(data.condition);
      ast_free(data.body);
      ast_free(data.elseClause);
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      ast_free(data.node);
      break;
    }
    case AST_CONST_DECL: {
      struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
      ast_free(data.identifier);
      ast_free(data.type);
      ast_free(data.expr);
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
    case AST_EXIT: {
      struct AST_EXIT data = ast.data.AST_EXIT;
      ast_free(data.value);
      break;
    }
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      ast_free(data.value);
      break;
    }
    case AST_CAST: {
      struct AST_CAST data = ast.data.AST_CAST;
      ast_free(data.identifier);
      ast_free(data.type);
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      ast_free(data.identifier);
      for (int i = 0; i < arrlen(data.arguments); i++) {
        ast_free(data.arguments[i]);
      }
      arrfree(data.arguments);
      break;
    }
    case AST_TYPE_DECL: {
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      for (int i = 0; i < arrlen(data.fields); i++) {
        ast_free(data.fields[i]);
      }
      arrfree(data.fields);
      break;
    }
    case AST_FN: {
      struct AST_FN data = ast.data.AST_FN;
      ast_free(data.identifier);
      ast_free(data.returnType);
      ast_free(data.body);

      for (int i = 0; i < arrlen(data.params); i++) {
        ast_free(data.params[i]);
      }
      arrfree(data.params);
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      ast_free(data.identifier);
      ast_free(data.type);
      break;
    }
    case AST_BLOCK: {
      struct AST_BLOCK data = ast.data.AST_BLOCK;
      ast_free(data.body);
      break;
    }
    case AST_LIST:
    {
      struct AST_LIST data = ast.data.AST_LIST;
      for (int i = 0; i < arrlen(data.decls); i++) {
        ast_free(data.decls[i]);
      }
      arrfree(data.decls);
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

const char* getNodeTypeName(AST_TAG tag) {
  switch(tag) {
    case AST_ERROR: return "ERROR";
    case AST_MAIN: return "MAIN";
    case AST_LIST: return "LIST";
    case AST_BLOCK: return "BLOCK";
    case AST_PARAM_LIST: return "PARAM_LIST";
    case AST_PARAM: return "PARAM";
    case AST_FN: return "FN";
    case AST_TYPE_DECL: return "TYPE_DECL";
    case AST_CALL: return "CALL";
    case AST_RETURN: return "RETURN";
    case AST_EXIT: return "EXIT";
    case AST_STMT: return "STMT";
    case AST_DECL: return "DECL";
    case AST_FOR: return "FOR";
    case AST_WHILE: return "WHILE";
    case AST_IF: return "IF";
    case AST_ASSIGNMENT: return "ASSIGNMENT";
    case AST_VAR_INIT: return "VAR_INIT";
    case AST_VAR_DECL: return "VAR_DECL";
    case AST_CONST_DECL: return "CONST_DECL";
    case AST_DOT: return "DOT";
    case AST_BINARY: return "BINARY_OP";
    case AST_UNARY: return "UNARY_OP";
    case AST_TYPE_NAME: return "TYPE_NAME";
    case AST_LVALUE: return "LVALUE";
    case AST_IDENTIFIER: return "IDENTIFIER";
    case AST_LITERAL: return "LITERAL";
    case AST_ASM: return "ASM";
    case AST_CAST: return "CAST";
  }
  return "UNKNOWN";
}
