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

#define BOOL_INDEX 2
#define U8_INDEX 3
#define I8_INDEX 4
#define U16_INDEX 5
#define I16_INDEX 6
#define FN_INDEX 9
#define NUMERICAL_INDEX 7
static bool isLiteral(int type) {
  return type == NUMERICAL_INDEX;
}
static bool isNumeric(int type) {
  return type <= NUMERICAL_INDEX && type >= 2;
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
static bool resolveFn(AST* ptr) {
  if (ptr != NULL && ptr->tag != AST_FN) {
    return false;
  }
  AST ast = *ptr;
  struct AST_FN data = ast.data.AST_FN;
  TYPE_TABLE_FIELD_ENTRY* entries = NULL;
  // Define symbol with parameter types
  bool r = true;
  for (int i = 0; i < arrlen(data.params); i++) {
    struct AST_PARAM param = data.params[i]->data.AST_PARAM;
    r &= traverse(data.params[i]);
    uint32_t index = data.params[i]->type;
    arrput(entries, ((TYPE_TABLE_FIELD_ENTRY){ NULL, index }));
  }
  r &= traverse(data.returnType);
  arrput(entries, ((TYPE_TABLE_FIELD_ENTRY){ NULL, data.returnType->type }));
  TYPE_TABLE_define(data.typeIndex, FN_INDEX, entries);

  return r;
}
static bool resolveType(AST* ptr) {
  if (ptr != NULL && ptr->tag != AST_TYPE_NAME) {
    return false;
  }
  AST ast = *ptr;
  struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
  TYPE_TABLE_FIELD_ENTRY* entries = NULL;
  // Define symbol with parameter types
  bool r = true;
  for (int i = 0; i < arrlen(data.components); i++) {
    struct AST_PARAM param = data.components[i]->data.AST_PARAM;
    r &= traverse(data.components[i]);
    uint32_t index = data.components[i]->type;
    arrput(entries, ((TYPE_TABLE_FIELD_ENTRY){ NULL, index }));
  }
  TYPE_TABLE_declare(data.typeName);
  return r;
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
        SYMBOL_TABLE_put(data.identifier, SYMBOL_TYPE_FUNCTION, data.typeIndex);
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
        bool r = traverse(data.type) && traverse(data.expr);
        int leftType = data.type->type;
        int rightType = data.expr->type;

        bool result = r && (leftType == rightType || (isNumeric(leftType) && isLiteral(rightType)));

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
        return leftType == rightType || (isNumeric(leftType) && isLiteral(rightType));
      }
    case AST_ASM:
      {
        ptr->type = 1;
        return true;
      }
    case AST_CAST:
      {
        struct AST_CAST data = ast.data.AST_CAST;
        bool r = traverse(data.identifier) && traverse(data.type);
        printf("%i\n", data.type->type);
        ptr->type = data.type->type;
        return r;
      }
    case AST_TYPE_NAME:
      {
        struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
        resolveType(ptr);
        if (arrlen(data.components) > 0) {
          for (int i = 0; i < arrlen(data.components); i++) {
            bool r = traverse(data.components[i]);
            if (!r) {
              return false;
            }
            int index = TYPE_TABLE_lookup(data.components[i]->data.AST_TYPE_NAME.typeName);
            ptr->type = index;
            if (index == 0) {
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
        r &= traverse(data.body);
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
              ptr->type = BOOL_INDEX; // to bool
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
          printf("%i vs %zu\n", leftType, typeTable[leftType].parent);
          return false;
        }

        /*
        // Call should contain it's arguments
        if (arrlen(entry.params) != arrlen(data.arguments)) {
          return false;
        }
        */
        for (int i = 0; i < arrlen(data.arguments); i++) {
          r = traverse(data.arguments[i]);
          if (!r) {
            return false;
          }
          // TODO: check arg type node against expected type of param
        }
        size_t fieldCount = arrlen(typeTable[leftType].fields);
        ptr->type = typeTable[leftType].fields[fieldCount - 1].typeIndex;
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

