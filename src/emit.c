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
#include "const_eval.h"
#include "platform.h"
#include "options.h"

PLATFORM p;

STRING** fnStack = NULL;
bool lvalue = false;

static void emitGlobal(FILE* f, AST* ptr) {
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        break;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        STRING* identifier = data.identifier;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        p.genGlobalVariable(f, symbol, EMPTY(), EMPTY());
        break;
      }
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        STRING* identifier = data.identifier;
        Value value = evalConstTree(data.expr);
        Value count = evalConstTree(data.type);
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        p.genGlobalVariable(f, symbol, value, count);
        break;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        STRING* identifier = data.identifier;
        Value value = evalConstTree(data.expr);
        Value count = evalConstTree(data.type);
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        p.genGlobalConstant(f, symbol, value, count);
        break;
      }
  }
}

AST** globals = NULL;
AST** functions = NULL;

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
        for (int i = 0; i < arrlen(data.modules); i++) {
          traverse(f, data.modules[i]);
        }
        for (int i = 0; i < arrlen(globals); i++) {
          emitGlobal(f, globals[i]);
        }
        p.genCompletePreamble(f);
        for (int i = 0; i < arrlen(functions); i++) {
          traverse(f, functions[i]);
          p.freeAllRegisters();
        }
        arrfree(functions);
        arrfree(globals);
        return 0;
      }
    case AST_MODULE:
      {
        struct AST_MODULE body = ast.data.AST_MODULE;
        for (int i = 0; i < arrlen(body.decls); i++) {
          if (body.decls[i]->tag == AST_FN) {
            arrput(functions, body.decls[i]);
          } else if (body.decls[i]->tag == AST_VAR_INIT) {
            arrput(globals, body.decls[i]);
          } else if (body.decls[i]->tag == AST_VAR_DECL) {
            arrput(globals, body.decls[i]);
          } else if (body.decls[i]->tag == AST_CONST_DECL) {
            arrput(globals, body.decls[i]);
          }
        }
        return 0;
      }
    case AST_BLOCK:
      {
        struct AST_BLOCK data = ast.data.AST_BLOCK;
        for (int i = 0; i < arrlen(data.decls); i++) {
          traverse(f, data.decls[i]);
          p.freeAllRegisters();
        }
        return 0;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        SYMBOL_TABLE_SCOPE scope = SYMBOL_TABLE_getScope(ast.scopeIndex);
        p.genFunction(f, data.identifier, scope);
        arrput(fnStack, data.identifier);
        traverse(f, data.body);

        if (strcmp(data.identifier->chars, "main") == 0) {
          struct AST_BLOCK block = data.body->data.AST_BLOCK;
          if (arrlen(block.decls) > 0) {
            size_t index = arrlen(block.decls) - 1;
            if (block.decls[index]->tag != AST_RETURN) {
              p.genReturn(f, fnStack[0], -1);
            }
          }
        }

        arrdel(fnStack, 0);

        p.genFunctionEpilogue(f, data.identifier, scope);
        if (strcmp(data.identifier->chars, "main") == 0) {
          p.genRunMain(f);
          p.genSimpleExit(f);
        }
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
          p.freeAllRegisters();
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
    case AST_DO_WHILE:
      {
        struct AST_DO_WHILE data = ast.data.AST_DO_WHILE;
        int loopLabel = p.labelCreate();
        p.genLabel(f, loopLabel);
        traverse(f, data.body);
        int r = traverse(f, data.condition);
        p.genCmpNotEqual(f, r, loopLabel);
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
    case AST_TYPE_FN:
    case AST_TYPE_PTR:
    case AST_TYPE_NAME:
      {
        return -1;
      }
    case AST_TYPE_ARRAY:
      {
        struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
        int r = traverse(f, data.subType);
        r = traverse(f, data.length);
        return r;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        // printf("%s: alloc %s\n", symbol.key, typeTable[symbol.typeIndex].entryType == ENTRY_TYPE_ARRAY ? "array" : "not array");
        int rvalue;
        int storage = traverse(f, data.type);
        if (storage != -1) {
          rvalue = p.genAllocStack(f, storage, typeTable[data.type->type].parent);
        } else {
          rvalue = p.genLoad(f, 0, 1);
        }

        return p.genInitSymbol(f, symbol, rvalue);
      }
    case AST_VAR_INIT:
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        // TODO: if in top level, it should be a static constant
        // otherwise treat it as a variable initialisation
        int rvalue;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        if (data.expr->tag == AST_INITIALIZER) {
          int dataType = typeTable[data.type->type].parent;
          int storageReg = traverse(f, data.type);
          rvalue = p.genAllocStack(f, storageReg, dataType);
          // TODO: initialiser
          struct AST_INITIALIZER init = data.expr->data.AST_INITIALIZER;

          for (int i = 0; i < arrlen(init.assignments); i++) {
            p.holdRegister(rvalue);
            int value = traverse(f, init.assignments[i]);
            int index = p.genLoad(f, i, 1);
            int slot = p.genIndexAddr(f, rvalue, index, dataType);
            int assign = p.genAssign(f, slot, value, dataType);
            p.freeRegister(assign);
          }
          p.freeRegister(rvalue);
        } else {
          rvalue = traverse(f, data.expr);
        }

        return p.genInitSymbol(f, symbol, rvalue);
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        int r = traverse(f, data.expr);
        int l = traverse(f, data.lvalue);
        return p.genAssign(f, l, r, data.lvalue->type);
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        int r;
        if (ast.rvalue) {
          r = p.genIdentifier(f, symbol);
        } else {
          r = p.genIdentifierAddr(f, symbol);
        }
        return r;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        Value v = data.value; //CONST_TABLE_get(data.constantIndex);

        // TODO handle different value types here
        int r = -1;
        if (IS_STRING(v)) {
          r = p.genConstant(f, data.constantIndex);
        } else if (IS_PTR(v)) {
          r = p.genConstant(f, AS_PTR(v));
        } else {
          int type = ptr->type;
          r = p.genLoad(f, AS_LIT_NUM(v), type);
        }
        return r;
      }
    case AST_REF:
      {
        struct AST_REF data = ast.data.AST_REF;
        STRING* identifier = data.expr->data.AST_IDENTIFIER.identifier;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        return p.genIdentifierAddr(f, symbol);
      }
    case AST_DEREF:
      {
        struct AST_DEREF data = ast.data.AST_DEREF;
        int r = traverse(f, data.expr);
        if (ast.rvalue) {
          return p.genDeref(f, r);
        } else {
          return r;
        }
      }
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        int r = traverse(f, data.expr);
        switch (data.op) {
          case OP_BITWISE_NOT: return p.genBitwiseNot(f, r);
          case OP_NOT: return p.genLogicalNot(f, r);
          case OP_NEG: return p.genNeg(f, r);
          default:
            {
              // unreachable
              printf("unknown unary operator\n");
              exit(1);
            }
        }
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        if (data.op == OP_AND) {
          // Short circuiting semantics please
          int doneLabel = p.labelCreate();
          int falseLabel = p.labelCreate();
          int l = traverse(f, data.left);
          p.genCmp(f, l, falseLabel);
          // if (!l), go to done
          int r = traverse(f, data.right);
          // if (!r) go to done
          p.genCmp(f, r, falseLabel);
          r = p.genLoad(f, 1, 1);
          p.genJump(f, doneLabel);
          p.genLabel(f, falseLabel);
          r = p.genLoadRegister(f, 0, r);
          p.genLabel(f, doneLabel);

          return r;
        } else if (data.op == OP_OR) {
          // Short circuiting semantics please
          int doneLabel = p.labelCreate();
          int trueLabel = p.labelCreate();
          int l = traverse(f, data.left);
          p.genCmpNotEqual(f, l, trueLabel);
          // if (l), go to done
          int r = traverse(f, data.right);
          // if (r) go to done
          p.genCmpNotEqual(f, r, trueLabel);
          r = p.genLoad(f, 0, 1);
          p.genJump(f, doneLabel);
          p.genLabel(f, trueLabel);
          r = p.genLoadRegister(f, 1, r);
          p.genLabel(f, doneLabel);

          return r;
        }

        int l = traverse(f, data.left);
        int r = traverse(f, data.right);
        switch (data.op) {
          case OP_ADD:
            {
              return p.genAdd(f, l, r, ptr->type);
            }
          case OP_SUB:
            {
              return p.genSub(f, l, r);
            }
          case OP_MUL:
            {
              return p.genMul(f, l, r, ptr->type);
            }
          case OP_DIV:
            {
              return p.genDiv(f, l, r, ptr->type);
            }
          case OP_MOD:
            {
              return p.genMod(f, l, r);
            }
          case OP_BITWISE_AND:
            {
              return p.genBitwiseAnd(f, l, r);
            }
          case OP_BITWISE_OR:
            {
              return p.genBitwiseOr(f, l, r);
            }
          case OP_BITWISE_XOR:
            {
              return p.genBitwiseXor(f, l, r);
            }
          case OP_SHIFT_LEFT:
            {
              return p.genShiftLeft(f, l, r);
            }
          case OP_SHIFT_RIGHT:
            {
              return p.genShiftRight(f, l, r);
            }
          case OP_NOT_EQUAL:
            {
              int doneLabel = p.labelCreate();
              int trueLabel = p.labelCreate();
              r = p.genSub(f, l, r);
              p.genCmp(f, r, trueLabel);
              r = p.genLoad(f, 1, 1);
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
              r = p.genLoad(f, 0, 1);
              p.genJump(f, doneLabel);
              p.genLabel(f, trueLabel);
              r = p.genLoadRegister(f, 1, r);
              p.genLabel(f, doneLabel);
              return r;
            }
          case OP_LESS:
            {
              return p.genLessThan(f, l, r);
            }
          case OP_LESS_EQUAL:
            {
              return p.genEqualLessThan(f, l, r);
            }
          case OP_GREATER:
            {
              return p.genGreaterThan(f, l, r);
            }
          case OP_GREATER_EQUAL:
            {
              return p.genEqualGreaterThan(f, l, r);
            }
        }
      }
    case AST_SUBSCRIPT:
      {
        struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
        int index = traverse(f, data.index);
        int left = traverse(f, data.left);
        TYPE_TABLE_ENTRY type = typeTable[data.left->type];
        int typeIndex = type.parent;
        if (ast.rvalue) {
          return p.genIndexRead(f, left, index, typeIndex);
        } else {
          return p.genIndexAddr(f, left, index, typeIndex);
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
void emitTree(AST* ptr, PLATFORM platform) {
  p = platform;

  FILE* f = stdout;
  if (!options.toTerminal) {
    char* filename = options.outfile == NULL ? "file.S" : options.outfile;
    f = fopen(filename, "w");

    if (f == NULL)
    {
      printf("Error opening file!\n");
      exit(1);
    }
  }

  p.init();
  traverse(f, ptr);
  p.complete();
  fprintf(f, "\n");
  if (!options.toTerminal) {
    fclose(f);
  } else {
    fflush(stdout);
  }

  PLATFORM_shutdown();
}
