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

STR* fnStack = NULL;
uint32_t* rStack = NULL;
bool lvalue = false;
static bool isPointer(int type) {
  return TYPE_get(type).entryType == ENTRY_TYPE_POINTER || TYPE_get(type).entryType == ENTRY_TYPE_ARRAY || type == 8;
}

static void printEntry(TYPE_ENTRY entry) {
  if (entry.name == EMPTY_STRING) {
    printf("null entry?\n");
  }
  printf("%s\n", CHARS(entry.name));
}

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
        STR identifier = data.identifier;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        Value count = evalConstTree(data.type);
        p.genGlobalVariable(f, symbol, EMPTY(), count);
        break;
      }
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        STR identifier = data.identifier;
        Value value = evalConstTree(data.expr);
        Value count = evalConstTree(data.type);
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        p.genGlobalVariable(f, symbol, value, count);
        break;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        STR identifier = data.identifier;
        Value value = evalConstTree(data.expr);
        Value count = evalConstTree(data.type);
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        p.genGlobalConstant(f, symbol, value, count);
        break;
      }
    default: break;
  }
}

AST** globals = NULL;
AST** functions = NULL;

struct SECTION { STR name; AST** globals; AST** functions; };
struct SECTION* sections = NULL;

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

        for (int i = 0; i < arrlen(sections); i++) {
          struct SECTION section = sections[i];
          printf("One section %s\n", CHARS(section.name));
          // TODO: emit section
          for (int j = 0; j < arrlen(section.globals); j++) {
            emitGlobal(f, section.globals[j]);
          }
          p.genCompletePreamble(f);
          for (int j = 0; j < arrlen(section.functions); j++) {
            traverse(f, section.functions[j]);
            p.freeAllRegisters();
          }
          arrfree(section.functions);
          arrfree(section.globals);
        }
        arrfree(functions);
        arrfree(globals);
        arrfree(sections);
        return 0;
      }
    case AST_BANK:
      {
        struct AST_BANK body = ast.data.AST_BANK;
        // TODO: emit bank section code
        struct SECTION section = { body.name, NULL, NULL };
        for (int i = 0; i < arrlen(body.decls); i++) {
          if (body.decls[i]->tag == AST_FN) {
            arrput(section.functions, body.decls[i]);
          } else if (body.decls[i]->tag == AST_VAR_INIT) {
            arrput(section.globals, body.decls[i]);
          } else if (body.decls[i]->tag == AST_VAR_DECL) {
            arrput(section.globals, body.decls[i]);
          } else if (body.decls[i]->tag == AST_CONST_DECL) {
            arrput(section.globals, body.decls[i]);
          }
        }
        arrput(sections, section);
        return 0;
      }
    case AST_MODULE:
      {
        struct AST_MODULE body = ast.data.AST_MODULE;
        for (int i = 0; i < arrlen(body.decls); i++) {
          if (body.decls[i]->tag == AST_BANK) {
            traverse(f, body.decls[i]);
          } else if (body.decls[i]->tag == AST_FN) {
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

        if (strcmp(CHARS(data.identifier), "main") == 0) {
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
        if (strcmp(CHARS(data.identifier), "main") == 0) {
          p.genRunMain(f);
          p.genSimpleExit(f);
        }
        break;
      }
    case AST_ASM:
      {
        struct AST_ASM data = ast.data.AST_ASM;
        for (int i = 0; i < arrlen(data.strings); i++) {
          p.genRaw(f, CHARS(data.strings[i]));
        }
        break;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        int r = traverse(f, data.condition);
        int nextLabel = p.labelCreate();
        p.genEqual(f, r, nextLabel);
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
          p.genEqual(f, r, exitLabel);
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
        p.genNotEqual(f, r, loopLabel);
        return -1;
      }
    case AST_WHILE:
      {
        struct AST_WHILE data = ast.data.AST_WHILE;
        int loopLabel = p.labelCreate();
        int exitLabel = p.labelCreate();
        p.genLabel(f, loopLabel);
        int r = traverse(f, data.condition);
        p.genEqual(f, r, exitLabel);
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
        //SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        // printf("%s: alloc %s\n", symbol.key, TYPE_get(symbol.typeIndex).entryType == ENTRY_TYPE_ARRAY ? "array" : "not array");
        int rvalue = -1;
        int storage = traverse(f, data.type);
        // TODO: return to VLA
        if (storage != -1) {
          //rvalue = p.genAllocStack(f, storage, TYPE_getParentId(data.type->type));
        } else {
          //rvalue = p.genLoad(f, 0, 1);
        }

        return rvalue;
        //return p.genInitSymbol(f, symbol, rvalue);
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
          struct AST_INITIALIZER init = data.expr->data.AST_INITIALIZER;
          if (init.initType == INIT_TYPE_RECORD) {
            rvalue = p.genIdentifierAddr(f, symbol);
            PUSH(rStack, rvalue);
            traverse(f, data.expr);
            POP(rStack);
          } else if (init.initType == INIT_TYPE_ARRAY) {
            rvalue = p.genIdentifierAddr(f, symbol);
            PUSH(rStack, rvalue);
            traverse(f, data.expr);
            POP(rStack);
          } else {
            int baseReg = traverse(f, data.type);
            rvalue = baseReg;
          }
          return rvalue;
        } else if (TYPE_get(symbol.typeIndex).entryType == ENTRY_TYPE_RECORD ||
              TYPE_get(symbol.typeIndex).entryType == ENTRY_TYPE_ARRAY) {
          int l = p.genIdentifierAddr(f, symbol);
          rvalue = traverse(f, data.expr);
          return p.genCopyObject(f, l, rvalue, symbol.typeIndex);
        } else {
          rvalue = traverse(f, data.expr);
          return p.genInitSymbol(f, symbol, rvalue);
        }
      }
    case AST_INITIALIZER:
      {
        struct AST_INITIALIZER init = ast.data.AST_INITIALIZER;
        int rvalue = PEEK(rStack);
        p.holdRegister(rvalue);
        if (init.initType == INIT_TYPE_RECORD) {
          for (int i = 0; i < arrlen(init.assignments); i++) {
            p.holdRegister(rvalue);
            struct AST_PARAM field = init.assignments[i]->data.AST_PARAM;
            int fieldReg = p.genFieldOffset(f, rvalue, ast.type, field.identifier);
            PUSH(rStack, fieldReg);
            int value = traverse(f, field.value);
            POP(rStack);
            if (field.value->tag != AST_INITIALIZER) {
              int assign = p.genAssign(f, fieldReg, value, init.assignments[i]->type);
              p.freeRegister(assign);
            }
          }
        } else if (init.initType == INIT_TYPE_ARRAY) {
          int dataType = TYPE_getParentId(ast.type);
          for (int i = 0; i < arrlen(init.assignments); i++) {
            p.holdRegister(rvalue);
            int index = p.genLoad(f, i, 1);
            int slot = p.genIndexAddr(f, rvalue, index, dataType);
            PUSH(rStack, slot);
            int value = traverse(f, init.assignments[i]);
            POP(rStack);
            int assign = p.genAssign(f, slot, value, dataType);
            p.freeRegister(assign);
          }
        }
        return rvalue;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        int r = traverse(f, data.expr);
        int l = traverse(f, data.lvalue);
        if (TYPE_get(data.lvalue->type).entryType == ENTRY_TYPE_RECORD && TYPE_get(data.expr->type).entryType == ENTRY_TYPE_RECORD) {
          return p.genCopyObject(f, l, r, data.lvalue->type);
        }
        if (TYPE_get(data.lvalue->type).entryType == ENTRY_TYPE_ARRAY && TYPE_get(data.expr->type).entryType == ENTRY_TYPE_ARRAY) {
          return p.genCopyObject(f, l, r, data.lvalue->type);
        }
        return p.genAssign(f, l, r, data.lvalue->type);
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, data.identifier);
        if (!symbol.defined) {
          symbol = SYMBOL_TABLE_checkBanks(data.identifier);
        }
        int r;
        fprintf(f, "; %s\n", CHARS(data.identifier));
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
        STR identifier = data.expr->data.AST_IDENTIFIER.identifier;
        SYMBOL_TABLE_ENTRY symbol = SYMBOL_TABLE_get(ast.scopeIndex, identifier);
        return p.genIdentifierAddr(f, symbol);
      }
    case AST_DEREF:
      {
        struct AST_DEREF data = ast.data.AST_DEREF;
        int r = traverse(f, data.expr);
        int typeIndex = ast.type;
        int ptrType = data.expr->type;
        if (ast.rvalue) {
          return p.genDeref(f, r, typeIndex);
        }
        printEntry(TYPE_get(data.expr->type));

        if (TYPE_get(data.expr->type).entryType == ENTRY_TYPE_POINTER && (TYPE_get(typeIndex).entryType != ENTRY_TYPE_RECORD && TYPE_get(typeIndex).entryType != ENTRY_TYPE_ARRAY)) {
          r = p.genDeref(f, r, ptrType);
          typeIndex = TYPE_getParentId(ptrType);
        }
        return r;
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
          p.genEqual(f, l, falseLabel);
          // if (!l), go to done
          int r = traverse(f, data.right);
          // if (!r) go to done
          p.genEqual(f, r, falseLabel);
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
          p.genNotEqual(f, l, trueLabel);
          // if (l), go to done
          int r = traverse(f, data.right);
          // if (r) go to done
          p.genNotEqual(f, r, trueLabel);
          r = p.genLoad(f, 0, 1);
          p.genJump(f, doneLabel);
          p.genLabel(f, trueLabel);
          r = p.genLoadRegister(f, 1, r);
          p.genLabel(f, doneLabel);

          return r;
        }

        int l = traverse(f, data.left);
        int r = traverse(f, data.right);
        if (isPointer(data.left->type) || isPointer(data.right->type)) {
          if (isPointer(data.right->type)) {
            int swap = l;
            l = r;
            r = swap;
          }
          switch (data.op) {
            case OP_ADD:
              {
                int byteSize = p.getSize(TYPE_getParentId(ptr->type));
                int scale = p.genLoad(f, byteSize, ptr->type);
                r = p.genMul(f, r, scale, ptr->type);
                return p.genAdd(f, l, r, ptr->type);
              }
            case OP_SUB:
              {
                int byteSize = p.getSize(TYPE_getParentId(ptr->type));
                int scale = p.genLoad(f, byteSize, ptr->type);
                r = p.genMul(f, r, scale, ptr->type);
                return p.genSub(f, l, r, ptr->type);
              }
            default: break;
          }
        }
        switch (data.op) {
          case OP_ADD:
            {
              return p.genAdd(f, l, r, ptr->type);
            }
          case OP_SUB:
            {
              return p.genSub(f, l, r, ptr->type);
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
              r = p.genCmp(f, l, r);
              p.genEqual(f, r, trueLabel);
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
              r = p.genCmp(f, l, r);
              p.genEqual(f, r, trueLabel);
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
          default: break;
        }
      }
    case AST_DOT:
      {
        struct AST_DOT data = ast.data.AST_DOT;
        int left = traverse(f, data.left);
        TYPE_ENTRY entry = TYPE_get(data.left->type);
        TYPE_ID parent = TYPE_getParentId(data.left->type);
        TYPE_ID typeIndex = data.left->type;
        if (entry.entryType == ENTRY_TYPE_POINTER && TYPE_get(parent).entryType == ENTRY_TYPE_RECORD) {
          typeIndex = parent;
          entry = TYPE_get(typeIndex);
          parent = TYPE_getParentId(typeIndex);
        }
        TYPE_FIELD_ENTRY field;
        for (int i = 0; i < arrlen(entry.fields); i++) {
          if (entry.fields[i].name == data.name) {
            field = entry.fields[i];
            break;
          }
        }
        TYPE_ENTRY fieldEntry = TYPE_get(field.typeIndex);

        int r = p.genFieldOffset(f, left, typeIndex, data.name);
        if (TYPE_get(data.left->type).entryType == ENTRY_TYPE_POINTER && (fieldEntry.entryType != ENTRY_TYPE_RECORD && fieldEntry.entryType != ENTRY_TYPE_ARRAY)) {
          // r = p.genDeref(f, r, parent);
        }
        if (ast.rvalue) {
          if (fieldEntry.entryType == ENTRY_TYPE_RECORD || fieldEntry.entryType == ENTRY_TYPE_ARRAY) {
          // r = p.genDeref(f, r, parent);
            ptr->rvalue = false;
          } else {
            r = p.genDeref(f, r, ast.type);
          }
        }
        return r;
      }
    case AST_SUBSCRIPT:
      {
        struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
        TYPE_ID typeIndex = TYPE_getParentId(data.left->type);
        int left = traverse(f, data.left);
        int index = traverse(f, data.index);
        if (ast.rvalue) {
          if (TYPE_get(typeIndex).entryType != ENTRY_TYPE_ARRAY && TYPE_get(typeIndex).entryType != ENTRY_TYPE_RECORD) {
            left = p.genIndexRead(f, left, index, typeIndex);
          } else {
            left = p.genIndexAddr(f, left, index, typeIndex);
          }
        } else {
          left = p.genIndexAddr(f, left, index, typeIndex);
        }
        return left;
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        int l = traverse(f, data.identifier);
        int* rs = NULL;
        for (int i = 0; i < arrlen(data.arguments); i++) {
          int r = traverse(f, data.arguments[i]);
          arrput(rs, r);
        }
        int r = p.genFunctionCall(f, l, rs);
        arrfree(rs);
        return r;
      }
    default: break;
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
