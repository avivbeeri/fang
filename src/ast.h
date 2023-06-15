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
#include "symbol_table.h"


typedef struct AST AST; // Forward reference
typedef int32_t TYPE_INDEX;

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
  AST_TYPE,
  AST_TYPE_NAME,
  AST_TYPE_FN,
  AST_TYPE_ARRAY,
  AST_TYPE_PTR,
  AST_REF,
  AST_DEREF,
  AST_UNARY,
  AST_BINARY,
  AST_DOT,
  AST_CONST_DECL,
  AST_VAR_DECL,
  AST_VAR_INIT,
  AST_ASSIGNMENT,
  AST_IF,
  AST_WHILE,
  AST_DO_WHILE,
  AST_FOR,
  AST_BLOCK,
  AST_CALL,
  AST_SUBSCRIPT,
  AST_CAST,
  AST_RETURN,
  AST_FN,
  AST_TYPE_DECL,
  AST_PARAM,
  AST_BANK,
  AST_MODULE_DECL,
  AST_MODULE,
  AST_EXT,
  AST_MAIN
} AST_TAG;

typedef enum {
  INIT_TYPE_NONE,
  INIT_TYPE_RECORD,
  INIT_TYPE_ARRAY,
} INIT_TYPE;

struct AST {
  AST_TAG tag;
  union {
    struct AST_ERROR { int number; } AST_ERROR;
    struct AST_LITERAL { int constantIndex; Value value; } AST_LITERAL;
    struct AST_INITIALIZER { AST** assignments; INIT_TYPE initType; } AST_INITIALIZER;
    struct AST_IDENTIFIER { STR module; STR identifier; } AST_IDENTIFIER;

    struct AST_TYPE { AST* type; } AST_TYPE;
    struct AST_TYPE_NAME { STR module; STR typeName; } AST_TYPE_NAME;
    struct AST_TYPE_ARRAY { AST* length; AST* subType; } AST_TYPE_ARRAY;
    struct AST_TYPE_FN { AST** params; AST* returnType; } AST_TYPE_FN;
    struct AST_TYPE_PTR { AST* subType; } AST_TYPE_PTR;

    struct AST_REF { AST *expr; } AST_REF;
    struct AST_DEREF { AST *expr; } AST_DEREF;
    struct AST_UNARY { AST_OP op; AST *expr; } AST_UNARY;
    struct AST_BINARY { AST_OP op; AST *left; AST *right; } AST_BINARY;
    struct AST_DOT { AST *left; STR name; } AST_DOT;
    struct AST_IF { AST* condition; AST* body; AST* elseClause; } AST_IF;
    struct AST_WHILE { AST* condition; AST* body; } AST_WHILE;
    struct AST_DO_WHILE { AST* condition; AST* body; } AST_DO_WHILE;
    struct AST_FOR { AST* initializer; AST* condition; AST* increment; AST* body; } AST_FOR;
    struct AST_CALL { AST* identifier; AST** arguments; } AST_CALL;
    struct AST_SUBSCRIPT { AST* left; AST* index; } AST_SUBSCRIPT;
    struct AST_CAST { AST* expr; AST* type; } AST_CAST;
    struct AST_RETURN { AST* value; } AST_RETURN;
    struct AST_PARAM { STR identifier; AST* value;  } AST_PARAM;

    struct AST_ASSIGNMENT { AST* lvalue; AST* expr; } AST_ASSIGNMENT;
    struct AST_VAR_DECL { STR identifier; AST* type; } AST_VAR_DECL;
    struct AST_VAR_INIT { STR identifier; AST* type; AST* expr; } AST_VAR_INIT;
    struct AST_CONST_DECL { STR identifier; AST* type; AST* expr; } AST_CONST_DECL;

    struct AST_FN { STR identifier; AST** params; AST* returnType; AST* body; AST* fnType; int typeIndex; } AST_FN;
    struct AST_TYPE_DECL { STR name; AST** fields; } AST_TYPE_DECL;
    struct AST_ASM { STR* strings; } AST_ASM;
    struct AST_BLOCK { AST** decls; } AST_BLOCK;
    struct AST_BANK { STR annotation; AST** decls; } AST_BANK;
    struct AST_MODULE_DECL { STR name; } AST_MODULE_DECL;
    struct AST_EXT { SYMBOL_TYPE symbolType; STR identifier; AST* type; } AST_EXT;
    struct AST_MODULE { AST** decls; } AST_MODULE;
    struct AST_MAIN { AST** modules; } AST_MAIN;
  } data;
  TYPE_INDEX type;
  Token token;
  uint64_t id;
  uint32_t scopeIndex;
  bool rvalue;
};

AST* ast_new(AST ast);
void ast_free(AST* ptr);
const char* getNodeTypeName(AST_TAG tag);
#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}, 0})

#define AST_NEW_T(tag, t, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}, 0, t, 0})

#endif
