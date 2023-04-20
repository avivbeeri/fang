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
#include "value.h"
#include "const_table.h"

#define emitf(fmt, ...) do { fprintf(f, fmt, ##__VA_ARGS__); } while(0)
static FILE* f;


static bool freereg[4];
static char *regList[4] = { "X1", "X2", "X3", "X4" };

static void genPreamble() {
  emitf(".global _start\n");
  emitf(".align 2\n");
  emitf("_start:\n");
}
static void genExit(int r) {
  // Assumes return code is in reg r.
  emitf("mov X0, %s\n", regList[r]);
  emitf("mov X16, #1\n");
  emitf("svc 0\n");
}

static void genPostamble() {
  size_t bytes = 0;
  for (int i = 0; i < arrlen(constTable); i++) {
    if (bytes % 4 != 0) {
      emitf(".align %lu\n", 4 - bytes % 4);
    }
    emitf("const_%i: .ascii \"%s\"\n", i, AS_STRING(constTable[i].value));
    bytes += strlen(AS_STRING(constTable[i].value));
  }

}

static void freeAllRegisters() {
  for (int i = 0; i < sizeof(freereg); i++) {
    freereg[i] = true;
  }
}

static void freeRegister(int r) {
  if (freereg[r]) {
    printf("Double-freeing register, abort to check");
    exit(1);
  }
  freereg[r] = true;
}

static int allocateRegister() {
  for (int i = 0; i < sizeof(freereg); i++) {
    if (freereg[i]) {
      freereg[i] = false;
      return i;
    }
  }
  printf("Out of registers, abort");
  exit(1);
}

static int genLoad(int i) {
  // Load i into a register
  // return the register index
  int r = allocateRegister();
  emitf("mov %s, #%i\n", regList[r], i);
  return r;
}

static int genAdd(int leftReg, int rightReg) {
  emitf("add %s, %s, %s\n", regList[leftReg], regList[leftReg], regList[rightReg]);
  freeRegister(rightReg);
  return leftReg;
}

static int traverse(FILE* f, AST* ptr) {
  if (ptr == NULL) {
    return 0;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR: {
      break;
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      genPreamble();
      traverse(f, data.body);
      return 0;
      break;
    }
    case AST_EXIT: {
      struct AST_EXIT data = ast.data.AST_EXIT;
      int r = traverse(f, data.value);
      genExit(r);
      return r;
      break;
    }
    case AST_LIST: {
      AST* next = ptr;
      int r = 0;
      while (next != NULL) {
        struct AST_LIST data = next->data.AST_LIST;
        r = traverse(f, data.node);
        next = data.next;
      }
      return r;
      break;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      int r = traverse(f, data.node);
      emitf("\n");
      return r;
      break;
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      int r = traverse(f, data.node);
      return r;
      break;
    }
    case AST_ASM: {
      struct AST_ASM data = ast.data.AST_ASM;
      for (int i = 0; i < arrlen(data.strings); i++) {
        emitf("%s\n", data.strings[i]->chars);
      }
      break;
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      switch (data.value.type) {
        case VAL_INT: {
          return genLoad(AS_NUMBER(data.value));
        }
        case VAL_STRING: {
          // emit a constant value
          break;
        }
        case VAL_BOOL: {
          return genLoad(AS_BOOL(data.value) ? 1 : 0);
          break;
        }
      }
      return 0;
      break;
    }
    case AST_BINARY: {
      struct AST_BINARY data = ast.data.AST_BINARY;
      int leftReg = traverse(f, data.left);
      int rightReg = traverse(f, data.right);
      switch(data.op) {
        case OP_ADD: return genAdd(leftReg, rightReg);
                     /*
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
        */
      }
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
  return 0;
}
void emitTree(AST* ptr) {
  f = fopen("file.S", "w");
  if (f == NULL)
  {
      printf("Error opening file!\n");
      exit(1);
  }

  freeAllRegisters();
  traverse(f, ptr);
  genPostamble();
  fprintf(f, "\n");

  fclose(f);
}
