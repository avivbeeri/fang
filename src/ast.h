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

#ifndef ast_h
#define ast_h
#include "common.h"
#include "scanner.h"
#include "memory.h"
#include "value.h"


typedef struct AST AST; // Forward reference
typedef int TYPE_INDEX;

typedef enum AST_OP {
  OP_ADD,
  OP_NEG,
  OP_SUB,
  OP_MOD,
  OP_DIV,
  OP_MUL,
  OP_AND,
  OP_OR,
  OP_NOT,

  OP_REF,
  OP_DEREF,

  OP_GREATER,
  OP_LESS,
  OP_SHIFT_LEFT,
  OP_SHIFT_RIGHT,
  OP_BITWISE_AND,
  OP_BITWISE_OR,
  OP_BITWISE_XOR,
  OP_BITWISE_NOT, // do I need two nots?

  OP_COMPARE_EQUAL,
  OP_NOT_EQUAL,
  OP_GREATER_EQUAL,
  OP_LESS_EQUAL,
} AST_OP;

typedef enum {
  AST_ERROR,
  AST_ASM,
  AST_LITERAL,
  AST_INITIALIZER,
  AST_IDENTIFIER,
  AST_LVALUE,
  AST_TYPE_NAME,
  AST_UNARY,
  AST_BINARY,
  AST_DOT,
  AST_CONST_DECL,
  AST_VAR_DECL,
  AST_VAR_INIT,
  AST_ASSIGNMENT,
  AST_IF,
  AST_WHILE,
  AST_FOR,
  AST_BLOCK,
  AST_CALL,
  AST_CAST,
  AST_RETURN,
  AST_EXIT,
  AST_FN,
  AST_TYPE_DECL,
  AST_PARAM,
  AST_LIST,
  AST_MAIN,
} AST_TAG;

struct AST {
  AST_TAG tag;
  union {
    struct AST_ERROR { int number; } AST_ERROR;
    struct AST_LITERAL { int constantIndex; } AST_LITERAL;
    struct AST_INITIALIZER { AST** assignments; } AST_INITIALIZER;
    struct AST_IDENTIFIER { STRING* identifier; } AST_IDENTIFIER;
    struct AST_LVALUE { STRING* identifier; } AST_LVALUE;
    struct AST_TYPE_NAME { STRING* typeName; } AST_TYPE_NAME;
    struct AST_UNARY { AST_OP op; AST *expr; } AST_UNARY;
    struct AST_BINARY { AST_OP op; AST *left; AST *right; } AST_BINARY;
    struct AST_DOT { AST *left; STRING* name; } AST_DOT;
    struct AST_IF { AST* condition; AST* body; AST* elseClause; } AST_IF;
    struct AST_WHILE { AST* condition; AST* body; } AST_WHILE;
    struct AST_FOR { AST* initializer; AST* condition; AST* increment; AST* body; } AST_FOR;
    struct AST_CALL { AST* identifier; AST** arguments; } AST_CALL;
    struct AST_CAST { AST* identifier; AST* type; } AST_CAST;
    struct AST_RETURN { AST* value; } AST_RETURN;
    struct AST_EXIT { AST* value; } AST_EXIT;
    struct AST_PARAM { STRING* identifier; AST* type;  } AST_PARAM;

    struct AST_ASSIGNMENT { AST* lvalue; AST* expr; } AST_ASSIGNMENT;
    struct AST_VAR_DECL { STRING* identifier; AST* type; } AST_VAR_DECL;
    struct AST_VAR_INIT { STRING* identifier; AST* type; AST* expr; } AST_VAR_INIT;
    struct AST_CONST_DECL { STRING* identifier; AST* type; AST* expr; } AST_CONST_DECL;

    struct AST_FN { STRING* identifier; AST** params; AST* returnType; AST* body; } AST_FN;
    struct AST_TYPE_DECL { STRING* name; int index; AST** fields; } AST_TYPE_DECL;
    struct AST_ASM { STRING** strings; } AST_ASM;
    struct AST_BLOCK { AST* body; } AST_BLOCK;
    struct AST_LIST { AST** decls; } AST_LIST;
    struct AST_MAIN { AST* body; } AST_MAIN;
  } data;
  TYPE_INDEX type;
  Token token;
  uint64_t id;
};

AST* ast_new(AST ast);
void ast_free(AST* ptr);
const char* getNodeTypeName(AST_TAG tag);
#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}, 0})

#define AST_NEW_T(tag, t, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}, 0, t, 0})

#endif
