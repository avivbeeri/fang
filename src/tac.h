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

#ifndef tac_h
#define tac_h

#include "common.h"
#include "memory.h"
#include "ast.h"
#include "value.h"
#include "type_table.h"
#include "platform.h"

typedef enum {
  TAC_PURITY_UNKNOWN,
  TAC_PURITY_PURE,
  TAC_PURITY_IMPURE
} TAC_PURITY;

typedef enum {
  TAC_OP_ERROR,
  TAC_OP_NONE,
  TAC_OP_ADD,
  TAC_OP_NEG,
  TAC_OP_SUB,
  TAC_OP_MOD,
  TAC_OP_DIV,
  TAC_OP_MUL,
  TAC_OP_AND,
  TAC_OP_OR,
  TAC_OP_NOT,


  TAC_OP_GREATER,
  TAC_OP_LESS,
  TAC_OP_LSR,
  TAC_OP_LSL,
  TAC_OP_BITWISE_AND,
  TAC_OP_BITWISE_OR,
  TAC_OP_BITWISE_XOR,
  TAC_OP_BITWISE_NOT, // ???

  TAC_OP_EQUAL,
  TAC_OP_NOT_EQUAL,
  TAC_OP_GREATER_EQUAL,
  TAC_OP_LESS_EQUAL,
} TAC_OP_TYPE;

typedef enum {
  TAC_OPERAND_ERROR,
  TAC_OPERAND_NONE,
  TAC_OPERAND_LITERAL,
  TAC_OPERAND_VARIABLE,
  TAC_OPERAND_TEMPORARY,
  TAC_OPERAND_LABEL
} TAC_OPERAND_TYPE;

typedef struct TAC_OPERAND {
  TAC_OPERAND_TYPE tag;
  union {
    struct TAC_OPERAND_LITERAL { Value value; } TAC_OPERAND_LITERAL;
    struct TAC_OPERAND_TEMPORARY { uint32_t n; } TAC_OPERAND_TEMPORARY;
    struct TAC_OPERAND_VARIABLE { /* scope */ uint32_t scopeIndex; STR module; STR name; uint32_t index; TYPE_ID type; } TAC_OPERAND_VARIABLE;
    struct TAC_OPERAND_LABEL { uint32_t n; /* local? */} TAC_OPERAND_LABEL;
  } data;
} TAC_OPERAND;

typedef enum {
  TAC_TYPE_ERROR,
  TAC_TYPE_INIT,
  TAC_TYPE_COPY,
  TAC_TYPE_PHI,
  TAC_TYPE_IF_FALSE,
  TAC_TYPE_IF_TRUE,
  TAC_TYPE_GOTO,
  TAC_TYPE_LABEL,
  TAC_TYPE_CALL,
  TAC_TYPE_RETURN,
} TAC_TYPE;

typedef struct TAC {
  TAC_TYPE tag;

  TAC_OPERAND op1;
  TAC_OPERAND op2;
  TAC_OP_TYPE op;
  TAC_OPERAND op3;

  struct TAC* prev;
  struct TAC* next;
} TAC;

typedef struct TAC_BLOCK {
  uint32_t label;
  TAC* start;
  TAC* end;

  struct TAC_BLOCK* prev;
  struct TAC_BLOCK* next;
  struct TAC_BLOCK* branch;

  TAC_PURITY pure;
  bool reachable;
} TAC_BLOCK;

typedef struct {
  TAC_PURITY pure;
  bool used;
  uint32_t bank;
  STR module;
  STR name;

  TAC_BLOCK* start;
  uint32_t scopeIndex;
} TAC_FUNCTION;

typedef struct {
  STR module;
  STR name;
  TYPE_ID type;
  uint32_t size;
  bool constant;
} TAC_DATA;

typedef struct {
  uint32_t index;
  STR name;
  TAC_DATA* data;
  TAC_FUNCTION* functions;
} TAC_SECTION;

typedef struct {
  TAC_SECTION* sections;
} TAC_PROGRAM;

TAC_PROGRAM emitTAC(AST* ptr);
void TAC_free(TAC_PROGRAM program);
void TAC_dump(TAC_PROGRAM program);

#endif
