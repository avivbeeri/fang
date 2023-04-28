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
#include "parser.h"
#include "ast.h"
#include "print.h"
#include "const_table.h"
#include "type_table.h"
#include "symbol_table.h"

struct { uint32_t key; STRING* value; }* nodeIdents = NULL;
uint32_t* typeStack = NULL;
#define PUSH(type) do { arrput(typeStack, type); } while (false)
#define POP() do { arrdel(typeStack, arrlen(typeStack) - 1); } while (false)
#define PEEK() (arrlen(typeStack) == 0 ? 0 : typeStack[arrlen(typeStack) - 1])

#define BOOL_INDEX 2
#define U8_INDEX 3
#define I8_INDEX 4
#define U16_INDEX 5
#define I16_INDEX 6
#define NUMERICAL_INDEX 7
#define FN_INDEX 9
static bool isLiteral(int type) {
  return type == NUMERICAL_INDEX;
}
static bool isNumeric(int type) {
  return type <= NUMERICAL_INDEX && type >= 2;
}

static bool isCompatible(int type1, int type2) {
  return type1 == type2
    || (isNumeric(type1) && isLiteral(type2))
    || (isLiteral(type1) && isNumeric(type2))
    || (isLiteral(type1) && isLiteral(type2));
}

static int valueToType(Value value) {
  switch (value.type) {
    case VAL_ERROR: return 0;
    case VAL_UNDEF: return 1; // void
    case VAL_BOOL: return BOOL_INDEX;

    case VAL_CHAR: return U8_INDEX;
    case VAL_U8: return U8_INDEX;

    case VAL_I8: return I8_INDEX;

    case VAL_U16: return U16_INDEX;
    case VAL_PTR: return U16_INDEX;

    case VAL_I16: return I16_INDEX;
    case VAL_LIT_NUM: return NUMERICAL_INDEX;
    case VAL_STRING: return 8;
    case VAL_RECORD: return 0;
    // Open to implicit num casting
  }
  return 0;
}

static bool traverse(AST* ptr);
static int resolveType(AST* ptr) {
  if (ptr == NULL) {
    return 0;
  }
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        int i = resolveType(data.type);
        ptr->type = i;
        return i;
      }
    case AST_TYPE_NAME:
      {
        struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
        int i = TYPE_TABLE_lookup(data.typeName);
        ptr->type = i;
        return i;
      }
    case AST_TYPE_PTR:
      {
        struct AST_TYPE_PTR data = ast.data.AST_TYPE_PTR;
        int subType = resolveType(data.subType);
        // Get type name from typetable
        // prepend ^
        // store in type table and record index
        STRING* name = typeTable[subType].name;
        STRING* typeName = STRING_prepend(name, "^");
        ptr->type = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, 2, subType, NULL);
        return ptr->type;
      }
    case AST_TYPE_FN:
      {
        struct AST_TYPE_FN data = ast.data.AST_TYPE_FN;
        TYPE_TABLE_FIELD_ENTRY* entries = NULL;
        // generate the fn type string here
        size_t len = 4;
        size_t i = 0;
        char* buffer = ALLOC_STR(len);
        APPEND_STR(buffer, len, i, "fn (");
        for (int i = 0; i < arrlen(data.params); i++) {
          int index = resolveType(data.params[i]);
          APPEND_STR(buffer, len, i, "%s", typeTable[index].name->chars);
          arrput(entries, ((TYPE_TABLE_FIELD_ENTRY){ NULL, index }));
          if (i < arrlen(data.params) - 1) {
            APPEND_STR(buffer, len, i, ", ");
          }
        }
        APPEND_STR(buffer, len, i, "): ");
        int returnType = resolveType(data.returnType);
        APPEND_STR(buffer, len, i, "%s", typeTable[returnType].name->chars);

        STRING* typeName = copyString(buffer, i);
        FREE(char, buffer);
        ptr->type = TYPE_TABLE_declare(typeName);
        TYPE_TABLE_defineCallable(ptr->type, FN_INDEX, entries, returnType);
        return ptr->type;
      }
    case AST_TYPE_ARRAY:
      {
        // Arrays are basically just pointers with some runtime allocation
        // semantics
        struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
        traverse(data.length);
        int lenType = data.length->type;
        if (!isNumeric(lenType)) {
          return 0;
        }
        int subType = resolveType(data.subType);
        STRING* name = typeTable[subType].name;
        STRING* typeName = STRING_prepend(name, "^");
        ptr->type = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_ARRAY, 2, subType, NULL);
        return ptr->type;
      }
  }
  return 0;
}
static bool resolveFn(AST* ptr) {
  if (ptr != NULL && ptr->tag != AST_FN) {
    return false;
  }
  AST ast = *ptr;
  struct AST_FN data = ast.data.AST_FN;
  ptr->type = resolveType(data.fnType);

  return ptr->type;
}

static bool resolveTopLevel(AST* ptr) {
  if (ptr == NULL) {
    return true;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        return false;
      }
    case AST_MAIN:
      {
        struct AST_MAIN data = ast.data.AST_MAIN;
        return resolveTopLevel(data.body);
      }
    case AST_LIST:
      {
        bool r = true;
        struct AST_LIST data = ast.data.AST_LIST;
        int* deferred = NULL;
        for (int i = 0; i < arrlen(data.decls); i++) {
          if (data.decls[i]->tag == AST_FN) {
            // FN pointer type resolution needs to occur after other types
            // are resolved.
            arrput(deferred, i);
            continue;
          }
          r = resolveTopLevel(data.decls[i]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }

        for (int i = 0; i < arrlen(deferred); i++) {
          r = resolveFn(data.decls[deferred[i]]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }
        arrfree(deferred);
        return r;
      }
    case AST_TYPE_DECL:
      {
        struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
        STRING* identifier = data.name;
        int index = TYPE_TABLE_declare(identifier);
        TYPE_TABLE_FIELD_ENTRY* fields = NULL;
        // should we resolve type fields before main verification?

        for (int i = 0; i < arrlen(data.fields); i++) {
          struct AST_PARAM field = data.fields[i]->data.AST_PARAM;
          traverse(field.value);
          int index = field.value->type;
          if (index == 0) {
            arrfree(fields);
            printf("bail %s\n", field.identifier->chars);
            return false;
          }
          arrput(fields, ((TYPE_TABLE_FIELD_ENTRY){ .typeIndex = index, .name = field.identifier } ));
        }
        TYPE_TABLE_define(index, ENTRY_TYPE_RECORD, 0, fields);
        return true;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        SYMBOL_TABLE_put(data.identifier, SYMBOL_TYPE_FUNCTION, ast.type);
        return true;
      }
    default:
      {
        return true;
      }
  }
  return false;
}

static bool traverse(AST* ptr) {
  if (ptr == NULL) {
    return true;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        return false;
      }
    case AST_MAIN:
      {
        struct AST_MAIN data = ast.data.AST_MAIN;
        return traverse(data.body);
      }
    case AST_RETURN:
      {
        struct AST_RETURN data = ast.data.AST_RETURN;
        return traverse(data.value);
      }
    case AST_EXIT:
      {
        struct AST_EXIT data = ast.data.AST_EXIT;
        return traverse(data.value);
      }
    case AST_BLOCK:
      {
        struct AST_BLOCK data = ast.data.AST_BLOCK;
        SYMBOL_TABLE_openScope();
        bool r = traverse(data.body);
        SYMBOL_TABLE_closeScope();
        return r;
      }
    case AST_LIST:
      {
        bool r = true;
        struct AST_LIST data = ast.data.AST_LIST;
        int* deferred = NULL;
        for (int i = 0; i < arrlen(data.decls); i++) {
          // Hoist FN resolution until after the main code
          // for type-check reasons
          if (data.decls[i]->tag == AST_FN) {
            arrput(deferred, i);
            continue;
          }
          r = traverse(data.decls[i]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }

        for (int i = 0; i < arrlen(deferred); i++) {
          r = traverse(data.decls[deferred[i]]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }
        arrfree(deferred);
        return r;
      }
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        STRING* identifier = data.identifier;
        bool r  = traverse(data.type);
        int leftType = data.type->type;
        PUSH(leftType);
        r &= traverse(data.expr);
        POP();
        int rightType = data.expr->type;
        if (!r) {
          return false;
        }

        bool result = r && (leftType == rightType || (isNumeric(leftType) && isLiteral(rightType)));
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_VARIABLE, leftType);
        return result;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        STRING* identifier = data.identifier;
        bool r = traverse(data.type);
        int typeIndex = data.type->type;
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_VARIABLE, typeIndex);
        return r;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        STRING* identifier = data.identifier;
        bool r = traverse(data.type);
        int leftType = data.type->type;
        PUSH(leftType);
        r &= traverse(data.expr);
        int rightType = data.expr->type;
        POP();

        bool result = r && (leftType == rightType || (isNumeric(leftType) && isLiteral(rightType)));

        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_CONSTANT, leftType);
        return result;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        bool ident = traverse(data.lvalue);
        int leftType = data.lvalue->type;
        PUSH(leftType);
        bool expr = traverse(data.expr);
        POP();
        int rightType = data.expr->type;

        if (!(ident && expr)) {
          return false;
        }
        ptr->type = data.lvalue->type;
        return isCompatible(leftType, rightType);
      }
    case AST_ASM:
      {
        ptr->type = 1;
        return true;
      }
    case AST_CAST:
      {
        struct AST_CAST data = ast.data.AST_CAST;
        bool r = traverse(data.expr) && traverse(data.type);
        ptr->type = data.type->type;
        return r && isCompatible(data.expr->type, data.type->type);
      }
    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        ptr->type = resolveType(data.type);
        return ptr->type != 0;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        STRING* identifier = data.identifier;
        uint32_t* paramList = NULL;
        // Define symbol with parameter types
        bool r = true;

        SYMBOL_TABLE_openScope();
        for (int i = 0; i < arrlen(data.params); i++) {
          struct AST_PARAM param = data.params[i]->data.AST_PARAM;
          STRING* paramName = param.identifier;
          uint32_t index = data.params[i]->type;
          arrput(paramList, index);
          SYMBOL_TABLE_put(paramName, SYMBOL_TYPE_PARAMETER, 0);
        }
        // TODO: Store params in type table
        // TODO: push return type onto stack
        r &= traverse(data.body);
        // pop from stack
        SYMBOL_TABLE_closeScope();
        arrput(paramList, data.returnType->type);
        if (!r) {
          return false;
        }
        SYMBOL_TABLE_putFn(identifier, SYMBOL_TYPE_FUNCTION, data.typeIndex, paramList);
        return r;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        Value value = CONST_TABLE_get(data.constantIndex);
        ptr->type = valueToType(value);
        if (isCompatible(PEEK(), ptr->type)) {
          printf("compatible\n");
          ptr->type = PEEK();
        }
        return true;
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        bool result = SYMBOL_TABLE_scopeHas(identifier);
        if (!result) {
          errorAt(&ast.token, "Identifier was not found.");
          return false;
        }
        SYMBOL_TABLE_ENTRY entry = SYMBOL_TABLE_get(identifier);
        if (entry.defined) {
          ptr->type = entry.typeIndex;
        }
        return result;
      }
    case AST_INITIALIZER:
      {
        struct AST_INITIALIZER data = ast.data.AST_INITIALIZER;
        printf("%i\n", PEEK());
        if ((data.initType == INIT_TYPE_RECORD && typeTable[PEEK()].entryType != ENTRY_TYPE_RECORD)
            || (data.initType == INIT_TYPE_ARRAY && typeTable[PEEK()].entryType != ENTRY_TYPE_ARRAY)) {
          return false;
        }

        ptr->type = PEEK();
        /*
        int childType = 0;
        for (int i = 0; i < arrlen(data.assignments); i++) {
          bool r = traverse(data.assignments[i]);
          if (!r) {
            return false;
          }
        }
        */
        return true;
      }
    case AST_LVALUE:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        ptr->type = SYMBOL_TABLE_get(identifier).typeIndex;
        return SYMBOL_TABLE_scopeHas(identifier);
      }
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        bool r = traverse(data.expr);
        switch (data.op) {
          case OP_NEG:
            {
              ptr->type = data.expr->type;
              break;
            }
          case OP_NOT:
            {
              ptr->type = BOOL_INDEX; // to bool
              break;
            }
          case OP_DEREF:
            {
              // TODO: check tha what we are reffing is not a literal
              int subType = data.expr->type;
              ptr->type = typeTable[subType].parent;
              if (ptr->type == 0) {
                return false;
              }
              break;
            }
          case OP_REF:
            {
              // TODO: check tha what we are reffing is not a literal
              int subType = data.expr->type;
              STRING* name = typeTable[subType].name;
              STRING* typeName = STRING_prepend(name, "^");
              ptr->type = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, 2, subType, NULL);
              break;
            }
        }
        return r;
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        bool r = traverse(data.left);
        int leftType = data.left->type;
        PUSH(leftType);
        r &= traverse(data.right);
        POP();
        int rightType = data.right->type;
        if (!r) {
          return false;
        }
        bool compatible = true;


        switch (data.op) {
          // Arithmetic-only operators
          case OP_ADD:
          case OP_SUB:
          case OP_MOD:
          case OP_DIV:
          case OP_MUL:
            {
              if (!isNumeric(leftType) || !isNumeric(rightType)) {
                compatible = false;
              }

              if (isNumeric(leftType) && leftType == rightType) {
                ptr->type = leftType;
              }
              if (isNumeric(leftType) && isLiteral(rightType)) {
                ptr->type = leftType;
              }
              if (isLiteral(leftType) && isNumeric(rightType)) {
                ptr->type = rightType;
              }
              if (isLiteral(leftType) && isLiteral(rightType)) {
                // should compute largest type to fit both values
                // Hopefully, constant-folding removes this node
                ptr->type = NUMERICAL_INDEX;
              }
              break;
            }

          // Numerical ops
          case OP_SHIFT_LEFT:
          case OP_SHIFT_RIGHT:
          case OP_BITWISE_XOR:
          case OP_BITWISE_AND:
          case OP_BITWISE_OR:
            {
              // Allow numerical types
              if (leftType == rightType) {
                ptr->type = leftType;
              } else if (isNumeric(leftType) && isLiteral(rightType)) {
                ptr->type = leftType;
              } else if (isLiteral(leftType) && isNumeric(rightType)) {
                ptr->type = rightType;
              } else {
                compatible = false;
              }
              break;
            }

          // Logical ops, return a bool
          case OP_OR:
          case OP_AND:
          case OP_COMPARE_EQUAL:
          case OP_NOT_EQUAL:
            {
              // TODO: Do we allow comparing different types?
              ptr->type = 1;
              break;
            }
          case OP_GREATER_EQUAL:
          case OP_LESS_EQUAL:
          case OP_GREATER:
          case OP_LESS:
            {
              if (isNumeric(leftType) && leftType == rightType) {
                ptr->type = BOOL_INDEX;
              } else if (isNumeric(leftType) && isLiteral(rightType)) {
                ptr->type = BOOL_INDEX;
              } else if (isLiteral(leftType) && isNumeric(rightType)) {
                ptr->type = BOOL_INDEX;
              } else if (isLiteral(leftType) && isLiteral(rightType)) {
                ptr->type = BOOL_INDEX;
              } else {
                compatible = false;
              }
              break;
            }
        }

        return compatible;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        bool r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        if (data.elseClause != NULL) {
          r = traverse(data.elseClause);
          if (!r) {
            return false;
          }
        }
        return true;
      }
    case AST_WHILE:
      {
        struct AST_WHILE data = ast.data.AST_WHILE;
        bool r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        return true;
      }
    case AST_FOR:
      {
        struct AST_FOR data = ast.data.AST_FOR;
        SYMBOL_TABLE_openScope();
        bool r = traverse(data.initializer);
        if (!r) {
          return false;
        }
        r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        r = traverse(data.increment);
        if (!r) {
          return false;
        }
        SYMBOL_TABLE_closeScope();
        return true;
      }
    case AST_DOT:
      {
        struct AST_DOT data = ast.data.AST_DOT;
        bool r = traverse(data.left);
        // Need to pass type upwards to validate field name
        if (!r) {
          return false;
        }
        // TODO: check right for existance of field
        // TODO: left side needs to be a record
        return true;
      }
    case AST_SUBSCRIPT:
      {
        struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
        bool r = traverse(data.left);
        r &= traverse(data.index);
        if (!r || !isNumeric(data.index->type)) {
          return false;
        }

        int arrType = data.left->type;
        ptr->type = typeTable[arrType].parent;
        return ptr->type != 0;
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        bool r = traverse(data.identifier);
        if (!r) {
          return false;
        }
        // resolve data.identifier to string
        uint32_t leftType = data.identifier->type;
        if (typeTable[leftType].parent != FN_INDEX) {
          return false;
        }

        // Call should contain it's arguments
        if (arrlen(typeTable[leftType].fields) != arrlen(data.arguments)) {
          return false;
        }

        for (int i = 0; i < arrlen(data.arguments); i++) {
          r = traverse(data.arguments[i]);
          if (!r) {
            return false;
          }
          // TODO: check arg type node against expected type of param
        }
        ptr->type = typeTable[leftType].returnType;
        return true;
      }
    default:
      {
        return true;
      }
  }
  return false;
}


bool resolveTree(AST* ptr) {
  SYMBOL_TABLE_init();
  bool success = resolveTopLevel(ptr);
  if (!success) {
    return false;
  }
  success &= traverse(ptr);
  success &= TYPE_TABLE_calculateSizes();
  SYMBOL_TABLE_closeScope();
  TYPE_TABLE_report();
  SYMBOL_TABLE_report();
  return success;
}

