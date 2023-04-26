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
#include "environment.h"
#include "const_table.h"

static Value traverse(AST* ptr, Environment* context) {
  if (ptr == NULL) {
    return U8(0);
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR: {
      return ERROR(0);
    }
    case AST_MAIN: {
      struct AST_MAIN data = ast.data.AST_MAIN;
      return traverse(data.body, context);
    }
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      return traverse(data.value, context);
    }
    case AST_EXIT: {
      struct AST_EXIT data = ast.data.AST_EXIT;
      return traverse(data.value, context);
    }
    case AST_BLOCK: {
      struct AST_BLOCK data = ast.data.AST_BLOCK;
      Environment env = beginScope(context);
      return traverse(data.body, &env);
      endScope(&env);
    }
    case AST_LIST: {
      Value r;
      struct AST_LIST data = ast.data.AST_LIST;
      for (int i = 0; i < arrlen(data.decls); i++) {
        r = traverse(data.decls[i], context);
        if (IS_ERROR(r)) {
          return r;
        }
      }
      return r;
    }
    case AST_ASM: {
      return U8(0);
    }
    case AST_INITIALIZER: {
      struct AST_INITIALIZER data = ast.data.AST_INITIALIZER;
      for (int i = 0; i < arrlen(data.assignments); i++) {
        // Get name, get value
        /*
        if (IS_ERROR(r)) {
          return r;
        }
        */
      }
      return U8(0);
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      return CONST_TABLE_get(data.constantIndex); // data.value;
    }
    case AST_LVALUE: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      STRING* identifier = data.identifier;
      return STRING(identifier);
    }
    case AST_IDENTIFIER: {
      struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
      STRING* identifier = data.identifier;
      return getSymbol(context, identifier->chars);
    }
    case AST_UNARY: {
      struct AST_UNARY data = ast.data.AST_UNARY;
      Value value = traverse(data.expr, context);
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
    case AST_BINARY: {
      struct AST_BINARY data = ast.data.AST_BINARY;
      Value left = traverse(data.left, context);
      Value right = traverse(data.right, context);
      if (IS_ERROR(left)) {
        return left;
      }
      if (IS_ERROR(right)) {
        return right;
      }
      /*
      if (data.left->type != data.right->type) {
        printf("%s vs %s\n", getNodeTypeName(data.left->tag), getNodeTypeName(data.right->tag));
        return ERROR(0);
      }
      */
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
    case AST_CONST_DECL: {
      struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
      STRING* identifier = data.identifier;
      Value type = traverse(data.type, context);
      Value expr = traverse(data.expr, context);
      bool success = define(context, identifier->chars, expr, true);
      return success ? EMPTY() : ERROR(1);
    }
    case AST_VAR_DECL: {
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      STRING* identifier = data.identifier;
      Value type = traverse(data.type, context);
      define(context, identifier->chars, EMPTY(), false);
      return EMPTY();
    }

    case AST_VAR_INIT: {
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      STRING* identifier = data.identifier;
      Value type = traverse(data.type, context);
      Value expr = traverse(data.expr, context);
      define(context, identifier->chars, expr, false);
      return expr;
    }

    case AST_SUBSCRIPT: {
      struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
      Value identifier = traverse(data.left, context);
      Value index = traverse(data.index, context);
      // TODO: index using identifier
      return identifier;
    }
    case AST_ASSIGNMENT: {
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      Value identifier = traverse(data.lvalue, context);
      Value expr = traverse(data.expr, context);
      bool success = assign(context, AS_STRING(identifier)->chars, expr);
      return success ? expr : ERROR(1);
    }
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      Value condition = traverse(data.condition, context);
      Value value = EMPTY();
      Environment env = beginScope(context);
      if (isTruthy(condition)) {
        value = traverse(data.body, &env);
      } else if (data.elseClause != NULL) {
        value = traverse(data.elseClause, &env);
      }
      endScope(&env);
      return value;
    }
    case AST_WHILE: {
      struct AST_WHILE data = ast.data.AST_WHILE;
      Value condition = traverse(data.condition, context);
      Value value = EMPTY();
      while (isTruthy(condition)) {
        Environment env = beginScope(context);
        value = traverse(data.body, &env);
        endScope(&env);

        condition = traverse(data.condition, context);
      }
      return value;
    }
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      Value value = EMPTY();
      Environment forEnv = beginScope(context);
      Value initializer = traverse(data.initializer, &forEnv);
      Value condition = traverse(data.condition, &forEnv);
      while (isTruthy(condition)) {
        Environment env = beginScope(&forEnv);
        value = traverse(data.body, &env);
        endScope(&env);
        Value increment = traverse(data.increment, &forEnv);
        condition = traverse(data.condition, &forEnv);
      }
      endScope(&forEnv);

      return value;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      Value identifier = traverse(data.identifier, context);
      // Call should contain it's arguments


      // traverse(data.params, context);
      break;
    }
  /*
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      traverse(r, context);
      traverse(n, context);
      traverse(t, context);
      traverse(y, context);
      break;
    }
    case AST_TYPE_DECL: {
      struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
      traverse(r, context);
      traverse(s, context);
      break;
    }
    case AST_FN: {
      struct AST_FN data = ast.data.AST_FN;
      traverse(r, context);
      traverse(t, context);
      traverse(e, context);
      traverse(y, context);
      break;
    }
    case AST_CALL: {
      struct AST_CALL data = ast.data.AST_CALL;
      traverse(r, context);
      traverse(s, context);
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      traverse(r, context);
      traverse(e, context);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      char* str = ".";
      traverse(t, context);

      traverse(t, context);
      break;
    }
*/
    default: {
      return EMPTY();
    }
  }
  return ERROR(0);
}


void evalTree(AST* ptr) {
  Environment context = { NULL, NULL };
//  shdefault(context.values, ((ENV_ENTRY){ ERROR(0), true }));
  Value result = traverse(ptr, &context);

  printf("Interpreter result: ");
  printValue(result);
  printf(": ");
  printValueType(result);
  printf("\n");
}

