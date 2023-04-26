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
#include "const_table.h"
#include "type_table.h"
#include "symbol_table.h"


#define NUMERICAL_INDEX 9
static int valueToType(Value value) {
  switch (value.type) {
    case VAL_ERROR: return 0;
    case VAL_UNDEF: return 1; // void
    case VAL_BOOL: return 2;
    case VAL_CHAR: return 3;
    case VAL_U8: return 4;
    case VAL_I8: return 5;
    case VAL_U16: return 6;
    case VAL_I16: return 7;
    case VAL_LIT_NUM: return NUMERICAL_INDEX;
    case VAL_PTR: return 9;
    case VAL_STRING: return 10;
    case VAL_RECORD: return 0;
    // Open to implicit num casting
  }
  return 0;
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
        for (int i = 0; i < arrlen(data.decls); i++) {
          r = resolveTopLevel(data.decls[i]);
          if (!r) {
            return false;
          }
        }
        return r;
      }
    case AST_TYPE_DECL:
      {
        struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
        STRING* identifier = typeTable[data.index].name;
        TYPE_TABLE_FIELD_ENTRY* fields = NULL;

        for (int i = 0; i < arrlen(data.fields); i++) {
          struct AST_PARAM field = data.fields[i]->data.AST_PARAM;
          STRING* fieldName = field.identifier;
          STRING* fieldType = field.value->data.AST_TYPE_NAME.typeName;
          int index = TYPE_TABLE_lookup(fieldType);
          if (index == 0) {
            arrfree(fields);
            return false;
          }
          arrput(fields, ((TYPE_TABLE_FIELD_ENTRY){ .typeIndex = index, .name = fieldName } ));
        }
        TYPE_TABLE_define(data.index, 0, fields);
        return true;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        SYMBOL_TABLE_put(data.identifier, SYMBOL_TYPE_FUNCTION, 9);
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
        bool r = traverse(data.type) && traverse(data.expr);
        int leftType = data.type->type;
        int rightType = data.expr->type;

        bool result = r && (leftType == rightType || (leftType <= NUMERICAL_INDEX && rightType == NUMERICAL_INDEX));

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
        bool r = traverse(data.type) && traverse(data.expr);
        int leftType = data.type->type;
        int rightType = data.expr->type;

        bool result = r && (leftType == rightType || (leftType <= NUMERICAL_INDEX && rightType == NUMERICAL_INDEX));

        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_CONSTANT, leftType);
        return result;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        bool ident = traverse(data.lvalue);
        bool expr = traverse(data.expr);
        if (!(ident && expr)) {
          return false;
        }
        ptr->type = data.lvalue->type;
        int leftType = data.lvalue->type;
        int rightType = data.expr->type;
        return leftType == rightType || (leftType <= NUMERICAL_INDEX && rightType == NUMERICAL_INDEX);
      }
    case AST_ASM:
      {
        ptr->type = 1;
        return true;
      }
    case AST_TYPE_NAME:
      {
        struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
        if (arrlen(data.components) > 0) {
          for (int i = 0; i < arrlen(data.components); i++) {
            bool r = traverse(data.components[i]);
            if (!r) {
              return false;
            }
          }
        } else {
          int index = TYPE_TABLE_lookup(data.typeName);
          ptr->type = index;
          if (index == 0) {
            return false;
          }
        }
        return true;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        STRING* identifier = data.identifier;
        // Define symbol with parameter types
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_FUNCTION, 0);
        bool r = traverse(data.returnType);
        if (!r) {
          return false;
        }
        SYMBOL_TABLE_openScope();
        for (int i = 0; i < arrlen(data.params); i++) {
          struct AST_PARAM param = data.params[i]->data.AST_PARAM;
          STRING* paramName = param.identifier;
          STRING* paramType = param.value->data.AST_TYPE_NAME.typeName;
          int index = TYPE_TABLE_lookup(paramType);
          if (index == 0) {
            // arrfree(fields);
            return false;
          }
          SYMBOL_TABLE_put(paramName, SYMBOL_TYPE_PARAMETER, 0);
          // arrput(fields, ((TYPE_TABLE_FIELD_ENTRY){ .typeIndex = index, .name = fieldName } ));
        }
        // TODO: Store params in type table
        r = traverse(data.body);
        SYMBOL_TABLE_closeScope();
        return r;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        Value value = CONST_TABLE_get(data.constantIndex);
        ptr->type = valueToType(value);
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
        for (int i = 0; i < arrlen(data.assignments); i++) {
          bool r = traverse(data.assignments[i]);
          if (!r) {
            return false;
          }
        }
        // We need an identifier stack here
        // ptr->type = SYMBOL_TABLE_get(identifier).typeIndex;
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
            }
          case OP_NOT:
            {
              ptr->type = 2; // to bool
            }
        }
        return r;
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        bool r = traverse(data.left) && traverse(data.right);
        if (!r) {
          return false;
        }

        int leftType = data.left->type;
        int rightType = data.right->type;
        bool compatible = true;


        switch (data.op) {
          // Arithmetic-only operators
          case OP_ADD:
          case OP_SUB:
          case OP_MOD:
          case OP_DIV:
          case OP_MUL:
            {
              if (leftType > NUMERICAL_INDEX || rightType > NUMERICAL_INDEX || leftType == 0 || rightType == 0) {
                compatible = false;
              }

              if (leftType != 1 && leftType == rightType) {
                ptr->type = leftType;
              }
              if (leftType != NUMERICAL_INDEX && rightType == NUMERICAL_INDEX ) {
                ptr->type = leftType;

              }
              if (leftType == NUMERICAL_INDEX && rightType != NUMERICAL_INDEX ) {
                ptr->type = rightType;
              }
              if (leftType == NUMERICAL_INDEX && rightType == NUMERICAL_INDEX) {
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
              if (leftType <= NUMERICAL_INDEX && rightType <= NUMERICAL_INDEX) {
                ptr->type = leftType;
              } else {
                compatible = false;
              }
            }

          // Logical ops, return a bool
          case OP_OR:
          case OP_AND:
          case OP_COMPARE_EQUAL:
          case OP_NOT_EQUAL:
            {
              ptr->type = 1;
              break;
            }
          case OP_GREATER_EQUAL:
          case OP_LESS_EQUAL:
          case OP_GREATER:
          case OP_LESS:
            {
              if (leftType != 1 && leftType == rightType) {
                ptr->type = 1;
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
        return false;
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        bool r = traverse(data.identifier);
        if (!r) {
          return false;
        }
        // resolve data.identifier to string
        STRING* leftName = NULL;
        if (false && SYMBOL_TABLE_get(leftName).typeIndex != 11) {
          return false;
        }

        // Call should contain it's arguments
        for (int i = 0; i < arrlen(data.arguments); i++) {
          r = traverse(data.arguments[i]);
          if (!r) {
            return false;
          }
          // TODO: check arg type node against expected type of param
        }
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
  success &= TYPE_TABLE_calculateSizes();
  if (!success) {
    return false;
  }
  success &= traverse(ptr);
  SYMBOL_TABLE_closeScope();
  TYPE_TABLE_report();
  SYMBOL_TABLE_report();
  return success;
}

