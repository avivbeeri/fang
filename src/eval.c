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

static uint16_t getNumber(Value value) {
  // PRE: Value must be numeric
  switch (value.type) {
    case VAL_BOOL: AS_BOOL(value) ? 1: 0; break;
    case VAL_CHAR: return AS_CHAR(value);
    case VAL_U8: return AS_U8(value);
    case VAL_I8: return AS_I8(value);
    case VAL_I16: return AS_I16(value);
    case VAL_U16: return AS_U16(value);
    case VAL_PTR: return AS_PTR(value);
    case VAL_INT: return AS_NUMBER(value);
  }
  return -1;
}
static bool isEqual(Value left, Value right) {
  if (IS_NUMERICAL(left) != IS_NUMERICAL(right)) {
    return false;
  }
  if (IS_NUMERICAL(left)) {
    return getNumber(left) == getNumber(right);
  }
  STRING* leftStr = AS_STRING(left);
  STRING* rightStr = AS_STRING(right);
  if (leftStr->length != rightStr->length) {
    return false;
  }
  size_t len = leftStr->length;
  // TODO: After string interning, this can be changed to the index
  return memcmp(leftStr->chars, rightStr->chars, len);
}


static void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL: printf("%s", AS_BOOL(value) ? "true" : "false"); break;
    case VAL_CHAR: printf("%c", AS_CHAR(value)); break;
    case VAL_U8: printf("%hhu", AS_U8(value)); break;
    case VAL_I8: printf("%hhi", AS_I8(value)); break;
    case VAL_I16: printf("%hi", AS_I16(value)); break;
    case VAL_U16: printf("%hu", AS_U16(value)); break;
    case VAL_PTR: printf("$%hu", AS_PTR(value)); break;
    case VAL_INT: printf("%lli", AS_NUMBER(value)); break;
    case VAL_STRING: printf("%s", AS_STRING(value)->chars); break;
    case VAL_ERROR: printf("ERROR(%i)", AS_ERROR(value)); break;
  }
}

static bool isTruthy(Value value) {
  switch (value.type) {
    case VAL_BOOL: return AS_BOOL(value);
    case VAL_CHAR: return AS_CHAR(value) != 0;
    case VAL_U8: return AS_U8(value) != 0;
    case VAL_I8: return AS_I8(value) != 0;
    case VAL_I16: return AS_I16(value) != 0;
    case VAL_U16: return AS_U16(value) != 0;
    case VAL_PTR: return AS_PTR(value) != 0;
    case VAL_INT: return AS_NUMBER(value) != 0;
    case VAL_STRING: return (AS_STRING(value)->length) > 0;
    case VAL_ERROR: return false;
  }
  return false;
}

static Value traverse(AST* ptr, void* context) {
  if (ptr == NULL) {
    return NUMBER(0);
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
    case AST_LIST: {
      AST* next = ptr;
      Value r;
      while (next != NULL) {
        struct AST_LIST data = next->data.AST_LIST;
        r = traverse(data.node, context);
        if (IS_ERROR(r)) {
          return r;
        }
        next = data.next;
      }
      return r;
    }
    case AST_DECL: {
      struct AST_DECL data = ast.data.AST_DECL;
      return traverse(data.node, context);
    }
    case AST_STMT: {
      struct AST_STMT data = ast.data.AST_STMT;
      return traverse(data.node, context);
    }
    case AST_ASM: {
      return NUMBER(0);
    }
    case AST_LITERAL: {
      struct AST_LITERAL data = ast.data.AST_LITERAL;
      return data.value;
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
              return NUMBER(-AS_NUMBER(value));
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
      switch(data.op) {
        case OP_ADD:
          {
            if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
              return NUMBER(AS_NUMBER(left) + AS_NUMBER(right));
            }
            break;
          };
        case OP_SUB:
          {
            if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
              return NUMBER(AS_NUMBER(left) - AS_NUMBER(right));
            }
            break;
          };
        case OP_MUL:
          {
            if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
              return NUMBER(AS_NUMBER(left) * AS_NUMBER(right));
            }
            break;
          };
        case OP_DIV:
          {
            if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
              return NUMBER(AS_NUMBER(left) / AS_NUMBER(right));
            }
            break;
          };
        case OP_MOD:
          {
            if (IS_NUMERICAL(left) && IS_NUMERICAL(right)) {
              return NUMBER(AS_NUMBER(left) % AS_NUMBER(right));
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
                     /*
        case OP_BITWISE_OR: str = "|"; break;
        case OP_BITWISE_AND: str = "&"; break;
        case OP_SHIFT_LEFT: str = "<<"; break;
        case OP_SHIFT_RIGHT: str = ">>"; break;
        default: str = "MISSING"; break;
        */
      }
      break;
    }
                  /*
    case AST_WHILE: {
      struct AST_WHILE data = ast.data.AST_WHILE;
      traverse(n, context);
      traverse(y, context);
      break;
    }
    case AST_FOR: {
      struct AST_FOR data = ast.data.AST_FOR;
      traverse(r, context);
      traverse(n, context);
      traverse(t, context);
      traverse(y, context);
      break;
    }
    case AST_IF: {
      struct AST_IF data = ast.data.AST_IF;
      traverse(n, context);
      traverse(y, context);
      if (data.elseClause != NULL) {
        traverse(e, context);
      }
      break;
    }
    case AST_ASSIGNMENT: {
      struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
      traverse(r, context);
      traverse(r, context);
      break;
    }
    case AST_VAR_INIT: {
      struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
      traverse(r, context);
      traverse(e, context);
      traverse(r, context);
      break;
    }
    case AST_VAR_DECL: {
      struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
      traverse(r, context);
      traverse(e, context);
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
    case AST_RETURN: {
      struct AST_RETURN data = ast.data.AST_RETURN;
      if (data.value != NULL) {
        traverse(e, context);
      }
      break;
    }
    case AST_PARAM: {
      struct AST_PARAM data = ast.data.AST_PARAM;
      traverse(r, context);
      traverse(e, context);
      break;
    }
    case AST_PARAM_LIST: {
      AST* next = ptr;
      while (next != NULL) {
        struct AST_PARAM_LIST data = next->data.AST_PARAM_LIST;
        traverse(e, context);
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

      traverse(r, context);
      break;
    }
    case AST_DOT: {
      struct AST_DOT data = ast.data.AST_DOT;
      char* str = ".";
      traverse(t, context);

      traverse(t, context);
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
      traverse(t, context);
      traverse(t, context);
      break;
    }
*/
    default: {
      return ERROR(0);
    }
  }
}
void evalTree(AST* ptr) {
  void* context = NULL;
  Value result = traverse(ptr, context);

  printf("Interpreter result: ");
  printValue(result);
  printf("\n");
}

