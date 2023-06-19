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

void AST_free(AST *ptr) {
  if (ptr == NULL) {
    return;
  }
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_ERROR: {
      break;
    }
    case AST_INITIALIZER: {
      struct AST_INITIALIZER data = ast.data.AST_INITIALIZER;
      for (int i = 0; i < arrlen(data.assignments); i++) {
        AST_free(data.assignments[i]);
      }
      arrfree(data.assignments);
      break;
    }
    case AST_TYPE: {
      struct AST_TYPE data = ast.data.AST_TYPE;
      AST_free(data.type);
      break;
    }
    case AST_TYPE_ARRAY: {
      struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
      AST_free(data.length);
      AST_free(data.subType);
      break;
    }
    case AST_TYPE_FN: {
      struct AST_TYPE_FN data = ast.data.AST_TYPE_FN;
      AST_free(data.returnType);
      for (int i = 0; i < arrlen(data.params); i++) {
        AST_free(data.params[i]);
      }
      arrfree(data.params);
      break;
    }
    case AST_TYPE_PTR: {
      struct AST_TYPE_PTR data = ast.data.AST_TYPE_PTR;
      AST_free(data.subType);
      break;
    }
    case AST_REF: {
      struct AST_REF data = ast.data.AST_REF;
      AST_free(data.expr);
      break;
    }
    case AST_DEREF: {
      struct AST_DEREF data = ast.data.AST_DEREF;
      AST_free(data.expr);
      break;
    }
    case AST_UNARY: {
      struct AST_UNARY data = ast.data.AST_UNARY;
      AST_free(data.expr);
      break;
    }
    case AST_BINARY: {
      struct AST_BINARY data = ast.data.AST_BINARY;
      AST_free(data.left);
      AST_free(data.right);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      AST_free(data.left);
      break;
    }
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      AST_free(data.condition);
      AST_free(data.body);
      AST_free(data.elseClause);
      break;
    }
    case AST_WHILE: {
      struct AST_WHILE data = ast.data.AST_WHILE;
      AST_free(data.condition);
      AST_free(data.body);
      break;
    }
    case AST_DO_WHILE: {
      struct AST_DO_WHILE data = ast.data.AST_DO_WHILE;
      AST_free(data.condition);
      AST_free(data.body);
      break;
    }
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      AST_free(data.initializer);
      AST_free(data.condition);
      AST_free(data.increment);
      AST_free(data.body);
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      AST_free(data.identifier);
      for (int i = 0; i < arrlen(data.arguments); i++) {
        AST_free(data.arguments[i]);
      }
      arrfree(data.arguments);
      break;
    }
    case AST_SUBSCRIPT: {
      struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
      AST_free(data.index);
      AST_free(data.left);
      break;
    }
    case AST_CAST: {
      struct AST_CAST data = ast.data.AST_CAST;
      AST_free(data.expr);
      AST_free(data.type);
      break;
    }
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      AST_free(data.value);
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      AST_free(data.value);
      break;
    }
    case AST_ASSIGNMENT: {
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      AST_free(data.lvalue);
      AST_free(data.expr);
      break;
    }
    case AST_VAR_DECL: {
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      AST_free(data.type);
      break;
    }
    case AST_VAR_INIT: {
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      AST_free(data.type);
      AST_free(data.expr);
      break;
    }
    case AST_CONST_DECL: {
      struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
      AST_free(data.type);
      AST_free(data.expr);
      break;
    }
    case AST_ISR: {
      struct AST_ISR data = ast.data.AST_ISR;
      AST_free(data.body);
      break;
    }
    case AST_FN: {
      struct AST_FN data = ast.data.AST_FN;
      AST_free(data.fnType);
      // params and returntype is shared between so we don't need to double-free
      AST_free(data.body);
      for (int i = 0; i < arrlen(data.params); i++) {
        free(data.params[i]);
      }
      arrfree(data.params);
      break;
    }
    case AST_TYPE_DECL: {
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      for (int i = 0; i < arrlen(data.fields); i++) {
        AST_free(data.fields[i]);
      }
      arrfree(data.fields);
      break;
    }
    case AST_UNION: {
      struct AST_UNION data = ast.data.AST_UNION;
      for (int i = 0; i < arrlen(data.fields); i++) {
        AST_free(data.fields[i]);
      }
      arrfree(data.fields);
      break;
    }
    case AST_ASM: {
      struct AST_ASM data = ast.data.AST_ASM;
      arrfree(data.strings);
      break;
    }
    case AST_BLOCK: {
      struct AST_BLOCK data = ast.data.AST_BLOCK;
      for (int i = 0; i < arrlen(data.decls); i++) {
        AST_free(data.decls[i]);
      }
      arrfree(data.decls);
      break;
    }
    case AST_BANK: {
      struct AST_BANK data = ast.data.AST_BANK;
      for (int i = 0; i < arrlen(data.decls); i++) {
        AST_free(data.decls[i]);
      }
      arrfree(data.decls);
      break;
    }
    case AST_EXT: {
      struct AST_EXT data = ast.data.AST_EXT;
      AST_free(data.type);
      break;
    }
    case AST_MODULE: {
      struct AST_MODULE data = ast.data.AST_MODULE;
      for (int i = 0; i < arrlen(data.decls); i++) {
        AST_free(data.decls[i]);
      }
      arrfree(data.decls);
      break;
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      for (int i = 0; i < arrlen(data.modules); i++) {
        AST_free(data.modules[i]);
      }
      arrfree(data.modules);
      break;
    }
    default: break;
  }
  free(ptr);
}

const char* getNodeTypeName(AST_TAG tag) {
  switch(tag) {
    case AST_MAIN: return "MAIN";
    case AST_MODULE: return "MODULE";
    case AST_MODULE_DECL: return "MODULE_DECL";
    case AST_BLOCK: return "BLOCK";
    case AST_PARAM: return "PARAM";
    case AST_FN: return "FN";
    case AST_TYPE_DECL: return "TYPE_DECL";
    case AST_CALL: return "CALL";
    case AST_RETURN: return "RETURN";
    case AST_FOR: return "FOR";
    case AST_WHILE: return "WHILE";
    case AST_DO_WHILE: return "DO WHILE";
    case AST_IF: return "IF";
    case AST_ASSIGNMENT: return "ASSIGNMENT";
    case AST_VAR_INIT: return "VAR_INIT";
    case AST_VAR_DECL: return "VAR_DECL";
    case AST_CONST_DECL: return "CONST_DECL";
    case AST_DOT: return "DOT";
    case AST_BINARY: return "BINARY_OP";
    case AST_REF: return "REF";
    case AST_DEREF: return "DEREF";
    case AST_UNARY: return "UNARY_OP";
    case AST_TYPE_NAME: return "TYPE_NAME";
    case AST_IDENTIFIER: return "IDENTIFIER";
    case AST_LITERAL: return "LITERAL";
    case AST_ASM: return "ASM";
    case AST_CAST: return "CAST";
    case AST_TYPE: return "TYPE";
    case AST_TYPE_FN: return "TYPE_FN";
    case AST_TYPE_PTR: return "TYPE_PTR";
    case AST_TYPE_ARRAY: return "TYPE_ARRAY";
    case AST_SUBSCRIPT: return "SUBSCRIPT";
    case AST_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}
