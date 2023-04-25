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
#include "type_table.h"
#include "symbol_table.h"

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
        bool r;
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
          STRING* fieldType = field.type->data.AST_TYPE_NAME.typeName;
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
        STRING* identifier = data.identifier;
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
        bool r;
        struct AST_LIST data = ast.data.AST_LIST;
        for (int i = 0; i < arrlen(data.decls); i++) {
          r = traverse(data.decls[i]);
          if (!r) {
            return r;
          }
        }
        return r;
      }
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        STRING* identifier = data.identifier;
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_VARIABLE, 0);
        return true;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        STRING* identifier = data.identifier;
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_VARIABLE, 0);
        return true;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        STRING* identifier = data.identifier;
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_CONSTANT, 0);
        return true;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        bool ident = traverse(data.lvalue);
        bool expr = traverse(data.expr);
        return ident && expr;
      }
    case AST_ASM:
      {
        return true;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        STRING* identifier = data.identifier;
        SYMBOL_TABLE_put(identifier, SYMBOL_TYPE_FUNCTION, 0);
        return true;
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        bool result = SYMBOL_TABLE_scopeHas(identifier);
        if (!result) {
          errorAt(&ast.token, "Identifier was not found.");
        }

        return result;
      }
    case AST_LVALUE:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        return SYMBOL_TABLE_scopeHas(identifier);
      }
      /*
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        Value value = traverse(data.expr);
        if (IS_ERROR(value)) {
          return value;
        }
        switch(data.op) {
          case OP_NEG:
            {
              if (IS_NUMERICAL(value)) {
                return getNumericalValue(-AS_NUMBER(value));
              }
            }
          case OP_NOT:
            {
              return BOOL_VAL(!isTruthy(value));
            }
            //case OP_REF: str = "$"; break;
            //case OP_DEREF: str = "@"; break;
        }
        return ERROR(0);
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        Value left = traverse(data.left);
        Value right = traverse(data.right);
        if (IS_ERROR(left)) {
          return left;
        }
        if (IS_ERROR(right)) {
          return right;
        }
        if (data.left->type != data.right->type) {
          printf("%s vs %s\n", getNodeTypeName(data.left->tag), getNodeTypeName(data.right->tag));
          return ERROR(0);
        }
        switch(data.op) {
          case OP_ADD:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) + AS_NUMBER(right));
              }
              break;
            };
          case OP_SUB:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) - AS_NUMBER(right));
              }
              break;
            };
          case OP_MUL:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) * AS_NUMBER(right));
              }
              break;
            };
          case OP_DIV:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) / AS_NUMBER(right));
              }
              break;
            };
          case OP_MOD:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) % AS_NUMBER(right));
              }
              break;
            };
          case OP_GREATER:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return BOOL_VAL(AS_NUMBER(left) > AS_NUMBER(right));
              }
              break;
            };
          case OP_LESS:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return BOOL_VAL(AS_NUMBER(left) < AS_NUMBER(right));
              }
              break;
            };
          case OP_GREATER_EQUAL:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return BOOL_VAL(AS_NUMBER(left) >= AS_NUMBER(right));
              }
              break;
            };
          case OP_LESS_EQUAL:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return BOOL_VAL(AS_NUMBER(left) <= AS_NUMBER(right));
              }
              break;
            };
          case OP_COMPARE_EQUAL:
            {
              return BOOL_VAL(isEqual(left, right));
            };
          case OP_NOT_EQUAL:
            {
              return BOOL_VAL(!isEqual(left, right));
            };
          case OP_OR:
            {
              return BOOL_VAL(isTruthy(left) || isTruthy(right));
            };
          case OP_AND:
            {
              return BOOL_VAL(isTruthy(left) && isTruthy(right));
            };
          case OP_SHIFT_LEFT:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) << AS_NUMBER(right));
              }
              break;
            };
          case OP_SHIFT_RIGHT:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) >> AS_NUMBER(right));
              }
              break;
            };
          case OP_BITWISE_OR:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) | AS_NUMBER(right));
              }
              break;
            };
          case OP_BITWISE_AND:
            {
              if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
                return getTypedNumberValue(left.type, AS_NUMBER(left) & AS_NUMBER(right));
              }
              break;
            };
            return ERROR(0);
        }
        break;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        Value identifier = traverse(data.identifier);
        Value type = traverse(data.type);
        Value expr = traverse(data.expr);
        bool success = define(context, AS_STRING(identifier)->chars, expr, true);
        return success ? EMPTY() : ERROR(1);
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        Value identifier = traverse(data.identifier);
        Value type = traverse(data.type);
        define(context, AS_STRING(identifier)->chars, EMPTY(), false);
        return EMPTY();
      }

    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        Value identifier = traverse(data.identifier);
        Value type = traverse(data.type);
        Value expr = traverse(data.expr);
        define(context, AS_STRING(identifier)->chars, expr, false);
        return expr;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        Value condition = traverse(data.condition);
        Value value = EMPTY();
        if (isTruthy(condition)) {
          value = traverse(data.body);
        } else if (data.elseClause != NULL) {
          value = traverse(data.elseClause);
        }
        return value;
      }
    case AST_WHILE:
      {
        struct AST_WHILE data = ast.data.AST_WHILE;
        Value condition = traverse(data.condition);
        Value value = EMPTY();
        while (isTruthy(condition)) {
          value = traverse(data.body);
          condition = traverse(data.condition);
        }
        return value;
      }
    case AST_FOR:
      {
        struct AST_FOR data = ast.data.AST_FOR;
        Value value = EMPTY();
        SYMBOL_TABLE_openScope();
        Value initializer = traverse(data.initializer);
        Value condition = traverse(data.condition);
        while (isTruthy(condition)) {
          value = traverse(data.body);
          Value increment = traverse(data.increment);
          condition = traverse(data.condition);
        }
        SYMBOL_TABLE_closeScope();

        return value;
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        Value identifier = traverse(data.identifier);
        // Call should contain it's arguments


        // traverse(data.params);
        break;
      }
    case AST_TYPE_DECL:
      {
        struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
        traverse(r);
        traverse(s);
        break;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        traverse(r);
        traverse(t);
        traverse(e);
        traverse(y);
        break;
      }
    case AST_PARAM:
      {
        struct AST_PARAM data = ast.data.AST_PARAM;
        traverse(r);
        traverse(e);
        break;
      }
    case AST_DOT:
      {
        struct AST_DOT data = ast.data.AST_DOT;
        char* str = ".";
        traverse(t);

        traverse(t);
        break;
      }
*/
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

