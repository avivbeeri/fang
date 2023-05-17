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
        if (data.decls[i]->tag == AST_FN ||
            data.decls[i]->tag == AST_ASM) {
          continue;
        }
        r = traverse(data.decls[i], context);
        if (IS_ERROR(r)) {
          return r;
        }
      }
      return r;
    }
    case AST_ASM: {
      return ERROR(0);
    }
    case AST_INITIALIZER: {
      struct AST_INITIALIZER data = ast.data.AST_INITIALIZER;
      if (data.initType == INIT_TYPE_ARRAY) {
        Value* values = NULL;
        for (int i = 0; i < arrlen(data.assignments); i++) {
          arrput(values, traverse(data.assignments[i], context));
        }
        return ARRAY(values);
      }
      return U8(0);
    }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        return CONST_TABLE_get(data.constantIndex);
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        return getSymbol(context, identifier->chars);
      }
    case AST_UNARY:
      {
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
      switch(data.op) {
        case OP_ADD:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) + AS_NUMBER(right));
          };
        case OP_SUB:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) - AS_NUMBER(right));
          };
        case OP_MUL:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) * AS_NUMBER(right));
          };
        case OP_DIV:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) / AS_NUMBER(right));
          };
        case OP_MOD:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) % AS_NUMBER(right));
          };
        case OP_GREATER:
          {
            return BOOL_VAL(AS_NUMBER(left) > AS_NUMBER(right));
          };
        case OP_LESS:
          {
            return BOOL_VAL(AS_NUMBER(left) < AS_NUMBER(right));
          };
        case OP_GREATER_EQUAL:
          {
            return BOOL_VAL(AS_NUMBER(left) >= AS_NUMBER(right));
          };
        case OP_LESS_EQUAL:
          {
            return BOOL_VAL(AS_NUMBER(left) <= AS_NUMBER(right));
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
            return getTypedNumberValue(left.type, AS_NUMBER(left) << AS_NUMBER(right));
          };
        case OP_SHIFT_RIGHT:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) >> AS_NUMBER(right));
          };
        case OP_BITWISE_OR:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) | AS_NUMBER(right));
          };
        case OP_BITWISE_AND:
          {
            return getTypedNumberValue(left.type, AS_NUMBER(left) & AS_NUMBER(right));
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
    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        return traverse(data.type, context);
      }
    case AST_TYPE_FN:
    case AST_TYPE_PTR:
    case AST_TYPE_NAME:
      {
        return EMPTY();
      }
    case AST_TYPE_ARRAY:
      {
        struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
        return traverse(data.length, context);
      }

    case AST_CAST:
      {
        struct AST_CAST data = ast.data.AST_CAST;
        return traverse(data.expr, context);
      }
    case AST_SUBSCRIPT: {
      struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
      Value identifier = traverse(data.left, context);
      Value index = traverse(data.index, context);
      // TODO: index using identifier
      return identifier;
    }
  }
  return ERROR(0);
}


Value evalConstTree(AST* ptr) {
  Environment context = { NULL, NULL };
  Value result = traverse(ptr, &context);
  return result;
}
