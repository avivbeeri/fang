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

#include "common.h"
#include "ast.h"

static void traverse(AST* ptr, int level) {
  if (ptr == NULL) {
    return;
  }
  AST ast = *ptr;
  //printf("%s\n", getNodeTypeName(ast.tag));
  switch(ast.tag) {
    case AST_ERROR: {
      printf("An error occurred in the tree");
      break;
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      traverse(data.node, level);
      break;
    }
    case AST_WHILE: {
      printf("%*s", level * 2, "");
      struct AST_WHILE data = ast.data.AST_WHILE;
      printf("while (");
      traverse(data.condition, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_FOR: {
      printf("%*s", level * 2, "");
      struct AST_FOR data = ast.data.AST_FOR;
      printf("for (");
      traverse(data.initializer, 0);
      printf("; ");
      traverse(data.condition, 0);
      printf("; ");
      traverse(data.increment, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_IF: {
      printf("%*s", level * 2, "");
      struct AST_IF data = ast.data.AST_IF;
      printf("if (");
      traverse(data.condition, 0);
      printf(") ");
      printf("{\n");
      traverse(data.body, level + 1);
      if (data.elseClause != NULL) {
        printf("%*s", level * 2, "");
        printf("} else {\n");
        traverse(data.elseClause, level + 1);
      }
      printf("%*s", level * 2, "");
      printf("}\n");
      break;
    }
    case AST_ASSIGNMENT: {
      printf("%*s", level * 2, "");
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      traverse(data.identifier, 0);
      printf(" = ");
      traverse(data.expr, 0);
      break;
    }
    case AST_VAR_INIT: {
      printf("%*s", level * 2, "");
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      printf("var ");
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      printf(" = ");
      traverse(data.expr, 0);
      break;
    }
    case AST_VAR_DECL: {
      printf("%*s", level * 2, "");
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      printf("var ");
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      break;
    }
    case AST_CONST_DECL: {
      printf("%*s", level * 2, "");
      struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
      printf("const ");
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      printf(" = ");
      traverse(data.expr, 0);
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      traverse(data.node, level);
      //printf("\n");
      break;
    }
    case AST_TYPE_DECL: {
      printf("%*s", level * 2, "");
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      printf("type ");
      traverse(data.identifier, 0);
      printf("{\n");
      for (int i = 0; i < arrlen(data.fields); i++) {
        traverse(data.fields[i], level + 1);
        printf("\n");
      }
      printf("\n%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_FN: {
      printf("%*s", level * 2, "");
      struct AST_FN data = ast.data.AST_FN;
      printf("fn ");
      traverse(data.identifier, 0);
      printf("(");
      for (int i = 0; i < arrlen(data.params); i++) {
        traverse(data.params[i], level + 1);
        if (i < arrlen(data.params) - 1) {
          printf(", ");
        }
      }
      printf("): ");
      traverse(data.returnType, 0);
      printf(" ");
      printf("{\n");
      traverse(data.body, level + 1);
      printf("%*s", level * 2, "");
      printf("}");
      break;
    }
    case AST_CAST: {
      struct AST_CAST data = ast.data.AST_CAST;
      traverse(data.identifier, 0);
      printf(" as ");
      traverse(data.type, 0);
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      traverse(data.identifier, 0);
      printf("(");
      for (int i = 0; i < arrlen(data.arguments); i++) {
        traverse(data.arguments[i], level + 1);
        if (i < arrlen(data.arguments) - 1) {
          printf(", ");
        }
      }
      printf(")");
      break;
    }
    case AST_EXIT: {
      printf("%*s", level * 2, "");
      struct AST_RETURN data = ast.data.AST_RETURN;
      printf("return ");
      if (data.value != NULL) {
        traverse(data.value, 0);
      }
      printf(";");
      break;
    }
    case AST_RETURN: {
      printf("%*s", level * 2, "");
      struct AST_RETURN data = ast.data.AST_RETURN;
      printf("return ");
      if (data.value != NULL) {
        traverse(data.value, 0);
      }
      printf(";");
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      traverse(data.identifier, 0);
      printf(": ");
      traverse(data.type, 0);
      break;
    }
    case AST_LIST: {
      struct AST_LIST data = ast.data.AST_LIST;
      for (int i = 0; i < arrlen(data.decls); i++) {
        traverse(data.decls[i], level);
        printf("\n");
      }
      break;
    }
    case AST_BLOCK: {
      struct AST_BLOCK data = ast.data.AST_BLOCK;
      return traverse(data.body, level);
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      printf("------ main --------\n");
      traverse(data.body, level);
      printf("------ complete --------\n");
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      switch (data.value.type) {
        case VAL_UNDEF: printf("0"); break;
        case VAL_ERROR: printf("ERROR"); break;
        case VAL_PTR: printf("$%hu", AS_PTR(data.value)); break;
        case VAL_U8: printf("%hhu", AS_U8(data.value)); break;
        case VAL_I8: printf("%hhi", AS_I8(data.value)); break;
        case VAL_I16: printf("%hi", AS_I16(data.value)); break;
        case VAL_U16: printf("%hu", AS_U16(data.value)); break;
        case VAL_STRING: printf("%s", AS_STRING(data.value)->chars); break;
        case VAL_BOOL: printf("%s", AS_BOOL(data.value) ? "true" : "false"); break;
        case VAL_CHAR: printf("%c", AS_CHAR(data.value)); break;

      }
      break;
    }
    case AST_TYPE_NAME: {
      struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
      printf("%s", data.typeName->chars);
      break;
    }
    case AST_ASM: {
      printf("%*s", level * 2, "");
      struct AST_ASM data = ast.data.AST_ASM;
      printf("ASM {\n");
      for (int i = 0; i < arrlen(data.strings); i++) {
        printf("%*s", (level + 1) * 2, "");
        printf("%s\n", data.strings[i]->chars);
      }
      printf("%*s", level * 2, "");
      printf("}\n");
      break;
    }
    case AST_LVALUE: {
      struct AST_LVALUE data = ast.data.AST_LVALUE;
      printf("%s", data.identifier->chars);
      break;
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      printf("%s", data.identifier->chars);
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
      printf("%s", str);
      traverse(data.expr, 0);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      char* str = ".";
      traverse(data.left, 0);
      printf("%s", str);
      traverse(data.right, 0);
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
      printf("(");
      traverse(data.left, 0);
      printf(" %s ", str);
      traverse(data.right, 0);
      printf(")");
      break;
    }
    default: {
      printf("\nERROR\n");
      break;
    }
  }
}
void traverseTree(AST* ptr) {
  traverse(ptr, 1);
  printf("\n");
}
