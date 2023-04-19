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
#include "ast.h"

#define emitf(fmt, ...) do { fprintf(f, fmt, ##__VA_ARGS__); } while(0)

static void traverse(FILE* f, AST* ptr) {
  if (ptr == NULL) {
    return;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR: {
      break;
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      emitf(".global _start\n");
      emitf(".align 2\n");
      emitf("_start:\n");
      traverse(f, data.body);
      break;
    }
    case AST_EXIT: {
      struct AST_EXIT data = ast.data.AST_EXIT;
      traverse(f, data.value);
      emitf("\n");
      emitf("mov X16, #1\n");
      emitf("svc 0\n");
      break;
    }
    case AST_LIST: {
      AST* next = ptr;
      while (next != NULL) {
        struct AST_LIST data = next->data.AST_LIST;
        traverse(f, data.node);
        next = data.next;
        emitf("\n");
      }
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      traverse(f, data.node);
      break;
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      traverse(f, data.node);
      break;
    }
    case AST_ASM: {
      struct AST_ASM data = ast.data.AST_ASM;
      traverse(f, data.strings);
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      traverse(f, data.value);
      break;
    }
    case AST_STRING: {
      struct AST_STRING data = ast.data.AST_STRING;
      emitf("%s", data.text->chars);
      break;
    }
    case AST_NUMBER: {
      struct AST_NUMBER data = ast.data.AST_NUMBER;
      emitf("mov X0, #%i", data.number);
      break;
    }
                  /*
    case AST_WHILE: {
      struct AST_WHILE data = ast.data.AST_WHILE;
      traverse(f, data.condition);
      traverse(f, data.body);
      break;
    }
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      traverse(f, data.initializer);
      traverse(f, data.condition);
      traverse(f, data.increment);
      traverse(f, data.body);
      break;
    }
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      traverse(f, data.condition);
      traverse(f, data.body);
      if (data.elseClause != NULL) {
        traverse(f, data.elseClause);
      }
      break;
    }
    case AST_ASSIGNMENT: {
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      traverse(f, data.identifier);
      traverse(f, data.expr);
      break;
    }
    case AST_VAR_INIT: {
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      traverse(f, data.identifier);
      traverse(f, data.type);
      traverse(f, data.expr);
      break;
    }
    case AST_VAR_DECL: {
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      traverse(f, data.identifier);
      traverse(f, data.type);
      break;
    }
    case AST_TYPE_DECL: {
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      traverse(f, data.identifier);
      traverse(f, data.fields);
      break;
    }
    case AST_FN: {
      struct AST_FN data = ast.data.AST_FN;
      traverse(f, data.identifier);
      traverse(f, data.paramList);
      traverse(f, data.returnType);
      traverse(f, data.body);
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      traverse(f, data.identifier);
      traverse(f, data.arguments);
      break;
    }
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      if (data.value != NULL) {
        traverse(f, data.value);
      }
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      traverse(f, data.identifier);
      traverse(f, data.type);
      break;
    }
    case AST_PARAM_LIST: {
      AST* next = ptr;
      while (next != NULL) {
        struct AST_PARAM_LIST data = next->data.AST_PARAM_LIST;
        traverse(f, data.node);
        next = data.next;
      }
      break;
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      break;
    }
    case AST_BOOL: {
      struct AST_BOOL data = ast.data.AST_BOOL;
      break;
    }
    case AST_NUMBER: {
      struct AST_NUMBER data = ast.data.AST_NUMBER;
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

      traverse(f, data.expr);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      char* str = ".";
      traverse(f, data.left);

      traverse(f, data.right);
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
      traverse(f, data.left);
      traverse(f, data.right);
      break;
    }
*/
    default: {
      break;
    }
  }
}
void emitTree(AST* ptr) {
  FILE *f = fopen("file.S", "w");
  if (f == NULL)
  {
      printf("Error opening file!\n");
      exit(1);
  }

  traverse(f, ptr);
  fprintf(f, "\n");

  fclose(f);
}