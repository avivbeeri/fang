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
#include "type_table.h"
#include "const_table.h"
#include "platform.h"

PLATFORM p;

STRING** fnStack = NULL;
bool lvalue = false;

static int traverse(FILE* f, AST* ptr) {
  if (ptr == NULL) {
    return 0;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        break;
      }
    case AST_MAIN:
      {
        struct AST_MAIN data = ast.data.AST_MAIN;
        p.genPreamble(f);
        p.genSimpleExit(f);
        traverse(f, data.body);
        return 0;
      }
    case AST_LIST:
      {
        struct AST_LIST data = ast.data.AST_LIST;
        int* deferred = NULL;
        for (int i = 0; i < arrlen(data.decls); i++) {
          if (data.decls[i]->tag == AST_FN) {
            arrput(deferred, i);
          } else {
            traverse(f, data.decls[i]);
            p.freeAllRegisters();
          }
        }
        for (int i = 0; i < arrlen(deferred); i++) {
          traverse(f, data.decls[deferred[i]]);
        }
        return 0;
      }
    case AST_BLOCK:
      {
        struct AST_BLOCK data = ast.data.AST_BLOCK;
        return traverse(f, data.body);
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        p.genFunction(f, data.identifier);
        arrput(fnStack, data.identifier);
        traverse(f, data.body);
        arrdel(fnStack, 0);
        p.genFunctionEpilogue(f, data.identifier);
        break;
      }
    case AST_ASM:
      {
        struct AST_ASM data = ast.data.AST_ASM;
        for (int i = 0; i < arrlen(data.strings); i++) {
          p.genRaw(f, data.strings[i]->chars);
        }
        break;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        int r = traverse(f, data.condition);
        int nextLabel = p.labelCreate();
        p.genCmp(f, r, nextLabel);
        traverse(f, data.body);

        if (data.elseClause != NULL) {
          int endLabel = p.labelCreate();
          p.genJump(f, endLabel);
          p.genLabel(f, nextLabel);
          traverse(f, data.elseClause);
          p.genLabel(f, endLabel);
        } else {
          p.genLabel(f, nextLabel);
        }
        return -1;
      }
    case AST_FOR:
      {
        struct AST_FOR data = ast.data.AST_FOR;
        int loopLabel = p.labelCreate();
        int exitLabel = p.labelCreate();
        if (data.initializer != NULL) {
          traverse(f, data.initializer);
          p.freeAllRegisters();
        }
        p.genLabel(f, loopLabel);
        if (data.condition != NULL) {
          int r = traverse(f, data.condition);
          p.genCmp(f, r, exitLabel);
        }
        traverse(f, data.body);
        p.freeAllRegisters();
        if (data.increment != NULL) {
          traverse(f, data.increment);
          p.freeAllRegisters();
        }
        p.genJump(f, loopLabel);
        p.genLabel(f, exitLabel);
        return -1;
      }
    case AST_WHILE:
      {
        struct AST_WHILE data = ast.data.AST_WHILE;
        int loopLabel = p.labelCreate();
        int exitLabel = p.labelCreate();
        p.genLabel(f, loopLabel);
        int r = traverse(f, data.condition);
        p.genCmp(f, r, exitLabel);
        traverse(f, data.body);
        p.genJump(f, loopLabel);
        p.genLabel(f, exitLabel);
        return -1;
      }
    case AST_RETURN:
      {
        struct AST_RETURN data = ast.data.AST_RETURN;
        int r = -1;
        if (data.value) {
          r = traverse(f, data.value);
        }
        p.genReturn(f, fnStack[0], r);
        return r;
      }

    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        return traverse(f, data.type);
      }
    case AST_CAST:
      {
        struct AST_CAST data = ast.data.AST_CAST;
        int r = traverse(f, data.expr);
        return r;
      }
    case AST_TYPE_ARRAY:
      {
        struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
        int r = traverse(f, data.length);
        return r;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        int rvalue = p.genLoad(f, 0);
        int r = p.genInitSymbol(f, symbol, rvalue);
        if (typeTable[ast.type].entryType == ENTRY_TYPE_ARRAY) {
          int storage = traverse(f, data.type);
          p.genAllocStack(f, r, storage);
        }
        p.freeRegister(r);
        return 0;
      }
    case AST_VAR_INIT:
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        // TODO: if in top level, it should be a static constant
        // otherwise treat it as a variable initialisation

        int rvalue = traverse(f, data.expr);
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        int r = p.genInitSymbol(f, symbol, rvalue);
        if (typeTable[ast.type].entryType == ENTRY_TYPE_ARRAY) {
          int storage = traverse(f, data.type);
          p.genAllocStack(f, r, storage);
        }
        // p.freeRegister(r);
        return 0;
      }
    case AST_LVALUE:
      {
        printf("LVALUE\n");
        struct AST_LVALUE data = ast.data.AST_LVALUE;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        int r = p.genIdentifierAddr(f, symbol);
        return r;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        int l = traverse(f, data.lvalue);
        int r = traverse(f, data.expr);
        return p.genAssign(f, l, r);
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        int r = p.genIdentifier(f, symbol);
        return r;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        Value v = CONST_TABLE_get(data.constantIndex);
        // TODO handle different value types here
        int r = p.genLoad(f, AS_LIT_NUM(v));
        return r;
      }
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        int r = traverse(f, data.expr);
        switch (data.op) {
          case OP_NOT: return p.genNeg(f, r);
          case OP_NEG: return p.genNeg(f, r);
        }
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        int l = traverse(f, data.left);
        int r = traverse(f, data.right);
        switch (data.op) {
          case OP_ADD:
            {
              return p.genAdd(f, l, r);
            }
          case OP_SUB:
            {
              return p.genSub(f, l, r);
            }
          case OP_MUL:
            {
              return p.genMul(f, l, r);
            }
          case OP_DIV:
            {
              return p.genDiv(f, l, r);
            }
          case OP_MOD:
            {
              return p.genMod(f, l, r);
            }
          case OP_NOT_EQUAL:
            {
              int doneLabel = p.labelCreate();
              int trueLabel = p.labelCreate();
              r = p.genSub(f, l, r);
              p.genCmp(f, r, trueLabel);
              r = p.genLoad(f, 1);
              p.genJump(f, doneLabel);
              p.genLabel(f, trueLabel);
              r = p.genLoadRegister(f, 0, r);
              p.genLabel(f, doneLabel);
              return r;
            }
          case OP_COMPARE_EQUAL:
            {
              int doneLabel = p.labelCreate();
              int trueLabel = p.labelCreate();
              r = p.genSub(f, l, r);
              p.genCmp(f, r, trueLabel);
              r = p.genLoad(f, 0);
              p.genJump(f, doneLabel);
              p.genLabel(f, trueLabel);
              r = p.genLoadRegister(f, 1, r);
              p.genLabel(f, doneLabel);
              return r;
            }
        }
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        int l = traverse(f, data.identifier);
        int* rs = NULL;
        for (int i = 0; i < arrlen(data.arguments); i++) {
          arrput(rs, traverse(f, data.arguments[i]));
        }
        int r = p.genFunctionCall(f, l, rs);
        arrfree(rs);
        return r;
      }
  }
  return 0;
}
void emitTree(AST* ptr) {
  PLATFORM_init();
  p = PLATFORM_get("apple_arm64");

  FILE* f = fopen("file.S", "w");
  if (f == NULL)
  {
      printf("Error opening file!\n");
      exit(1);
  }

  p.init();
  traverse(f, ptr);
  p.complete();
  fprintf(f, "\n");
  fclose(f);

  PLATFORM_shutdown();
}
