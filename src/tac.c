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

#include "tac.h"
#include "type_table.h"
#include "symbol_table.h"
#include "ast.h"

struct SECTION { STR name; STR annotation; AST** globals; AST** functions; bool bank; };

struct SECTION* prepareTree(AST* ptr) {
  struct SECTION* sections = NULL;
  struct AST_MAIN data = ptr->data.AST_MAIN;
  AST** banks = NULL;

  for (int i = 0; i < arrlen(data.modules); i++) {
    struct AST_MODULE body = data.modules[i]->data.AST_MODULE;

    struct SECTION section = {
      .name = SYMBOL_TABLE_getNameFromStart(data.modules[i]->scopeIndex),
      .annotation = EMPTY_STRING,
      .globals = NULL,
      .functions = NULL,
      .bank = false
    };
    for (int i = 0; i < arrlen(body.decls); i++) {
      if (body.decls[i]->tag == AST_BANK) {
        arrput(banks, body.decls[i]);
      } else if (body.decls[i]->tag == AST_ISR) {
        arrput(section.functions, body.decls[i]);
      } else if (body.decls[i]->tag == AST_FN) {
        arrput(section.functions, body.decls[i]);
      } else if (body.decls[i]->tag == AST_VAR_INIT) {
        arrput(section.globals, body.decls[i]);
      } else if (body.decls[i]->tag == AST_VAR_DECL) {
        arrput(section.globals, body.decls[i]);
      } else if (body.decls[i]->tag == AST_CONST_DECL) {
        arrput(section.globals, body.decls[i]);
      }
    }
    arrput(sections, section);
  }
  for (int i = 0; i < arrlen(banks); i++) {
    struct AST_BANK bank = banks[i]->data.AST_BANK;
    struct SECTION bankSection = { bank.name, bank.annotation, NULL, NULL, true };
    for (int i = 0; i < arrlen(bank.decls); i++) {
      if (bank.decls[i]->tag == AST_FN) {
        arrput(bankSection.functions, bank.decls[i]);
      } else if (bank.decls[i]->tag == AST_VAR_INIT) {
        arrput(bankSection.globals, bank.decls[i]);
      } else if (bank.decls[i]->tag == AST_VAR_DECL) {
        arrput(bankSection.globals, bank.decls[i]);
      } else if (bank.decls[i]->tag == AST_CONST_DECL) {
        arrput(bankSection.globals, bank.decls[i]);
      }
    }
    arrput(sections, bankSection);
  }
  arrfree(banks);

  return sections;
}

// Append instruction to end of block
static void TAC_addInstruction(TAC_BLOCK* context, TAC instruction) {
  TAC* end = context->end;

  TAC* entry = calloc(1, sizeof(TAC));
  *entry = instruction;

  entry->next = NULL;
  entry->prev = end;

  if (end != NULL) {
    end->next = entry;
  }
  context->end  = entry;
  TAC* start = context->start;
  if (start == NULL) {
    context->start = entry;
  }
}

uint64_t tempNo = 0;
uint64_t labelNo = 0;

static TAC_OPERAND labelOperand(uint64_t label) {
  TAC_OPERAND operand = (TAC_OPERAND) {
    .tag = TAC_OPERAND_LABEL,
    .data = { .TAC_OPERAND_LABEL = { .n = label }}
  };
  return operand;
}

static void TAC_emitIf(TAC_BLOCK* context, TAC_OPERAND operand, uint64_t label) {
  TAC_OPERAND branch = labelOperand(label);
  TAC_addInstruction(context, (TAC){
    .tag = TAC_TYPE_IF,
    .op1 = operand,
    .op2 = branch,
    .op = TAC_OP_NONE,
  });
}
static void TAC_emitJump(TAC_BLOCK* context, uint64_t label) {
  TAC_OPERAND operand = labelOperand(label);
  TAC_addInstruction(context, (TAC){
    .tag = TAC_TYPE_GOTO,
    .op1 = operand,
    .op = TAC_OP_NONE,
  });
}
static void TAC_emitLabel(TAC_BLOCK* context, uint64_t label) {
  TAC_OPERAND operand = labelOperand(label);
  TAC_addInstruction(context, (TAC){
    .tag = TAC_TYPE_LABEL,
    .op1 = operand,
    .op = TAC_OP_NONE,
  });
}


static TAC_OPERAND traverseExpr(TAC_BLOCK* context, AST* ptr) {
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        struct TAC_OPERAND_VARIABLE var = {
          .module = data.module,
          .name = data.identifier,
          .type = ast.type,
          .scopeIndex = ast.scopeIndex
        };
        TAC_OPERAND operand = (TAC_OPERAND){
          .tag = TAC_OPERAND_VARIABLE,
          .data = { .TAC_OPERAND_VARIABLE = var }
        };
        return operand;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        struct TAC_OPERAND_LITERAL lit = {
          .value = data.value
        };
        TAC_OPERAND operand = (TAC_OPERAND){
          .tag = TAC_OPERAND_LITERAL,
          .data = { .TAC_OPERAND_LITERAL = lit }
        };
        return operand;
      }
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        TAC_OPERAND r = traverseExpr(context, data.expr);
        struct TAC_OPERAND_TEMPORARY temp = {
          .n = tempNo++
        };
        TAC_OPERAND operand = (TAC_OPERAND){
          .tag = TAC_OPERAND_TEMPORARY,
          .data = { .TAC_OPERAND_TEMPORARY = temp }
        };
        TAC_OP_TYPE op = TAC_OP_ERROR;
        switch (data.op) {
          case OP_BITWISE_NOT: op = TAC_OP_BITWISE_NOT;
          case OP_NOT: op = TAC_OP_NOT;
          case OP_NEG: op = TAC_OP_NEG;
          default: break;
        }
        TAC_addInstruction(context, (TAC){
          .tag = TAC_TYPE_COPY,
          .op1 = operand,
          .op2 = r,
          .op = op,
        });
        return operand;
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        TAC_OPERAND l = traverseExpr(context, data.left);
        TAC_OPERAND r = traverseExpr(context, data.right);
        struct TAC_OPERAND_TEMPORARY temp = {
          .n = tempNo++
        };
        TAC_OPERAND operand = (TAC_OPERAND){
          .tag = TAC_OPERAND_TEMPORARY,
          .data = { .TAC_OPERAND_TEMPORARY = temp }
        };
        TAC_OP_TYPE op = TAC_OP_ERROR;
        switch (data.op) {
          case OP_ADD:
            op = TAC_OP_ADD; break;
          case OP_SUB:
            op = TAC_OP_SUB; break;
          case OP_MOD:
            op = TAC_OP_MOD; break;
          case OP_MUL:
            op = TAC_OP_MUL; break;
          case OP_DIV:
            op = TAC_OP_DIV; break;
          case OP_GREATER:
            op = TAC_OP_GREATER; break;
          case OP_LESS:
            op = TAC_OP_LESS; break;
          case OP_GREATER_EQUAL:
            op = TAC_OP_GREATER_EQUAL; break;
          case OP_LESS_EQUAL:
            op = TAC_OP_LESS_EQUAL; break;
          case OP_SHIFT_LEFT:
            op = TAC_OP_LSL; break;
          case OP_SHIFT_RIGHT:
            op = TAC_OP_LSR; break;
          case OP_BITWISE_AND:
            op = TAC_OP_BITWISE_AND; break;
          case OP_BITWISE_OR:
            op = TAC_OP_BITWISE_OR; break;
          default:
            break;
        }
        TAC_addInstruction(context, (TAC){
          .tag = TAC_TYPE_COPY,
          .op1 = operand,
          .op2 = l,
          .op3 = r,
          .op = op,
        });
        return operand;
      }
    default: break;
  }
  return (TAC_OPERAND){ .tag = TAC_OPERAND_ERROR };
}


static int traverseStmt(TAC_BLOCK* context, AST* ptr) {
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_BLOCK:
      {
        struct AST_BLOCK data = ast.data.AST_BLOCK;
        for (int i = 0; i < arrlen(data.decls); i++) {
          traverseStmt(context, data.decls[i]);
        }
        break;
      }
    case AST_CONST_DECL:
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        TAC_OPERAND r = traverseExpr(context, data.expr);
        struct TAC_OPERAND_VARIABLE var = {
          // TODO: put module on everything
         // .module = data.module,
          .name = data.identifier,
          .type = ast.type,
          .scopeIndex = ast.scopeIndex
        };
        TAC_OPERAND l = (TAC_OPERAND){
          .tag = TAC_OPERAND_VARIABLE,
          .data = { .TAC_OPERAND_VARIABLE = var }
        };

        TAC_addInstruction(context, (TAC){
          .tag = TAC_TYPE_COPY,
          .op1 = l,
          .op2 = r,
          .op = TAC_OP_NONE,
        });

        return -1;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;
        TAC_OPERAND r = traverseExpr(context, data.expr);
        TAC_OPERAND l = traverseExpr(context, data.lvalue);

        TAC_addInstruction(context, (TAC){
          .tag = TAC_TYPE_COPY,
          .op1 = l,
          .op2 = r,
          .op = TAC_OP_NONE,
        });

        return -1;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        TAC_OPERAND condition = traverseExpr(context, data.condition);
        uint32_t nextLabel = labelNo++;
        // TODO - branch on condition
        TAC_emitIf(context, condition, nextLabel);
        traverseStmt(context, data.body);

        if (data.elseClause != NULL) {
          // jump
          uint32_t endLabel = labelNo++;
          TAC_emitJump(context, endLabel);
          TAC_emitLabel(context, nextLabel);
          traverseStmt(context, data.elseClause);
          // emit label
          TAC_emitLabel(context, endLabel);
        } else {
          TAC_emitLabel(context, nextLabel);
        }
        return -1;
      }
    default:
      {
        // Default stmt is an expression
        traverseExpr(context, ptr);
      }
  }
  return -1;
}

static TAC_BLOCK* TAC_generateBasicBlocks(AST* start) {
  TAC_BLOCK* block = calloc(1, sizeof(TAC_BLOCK));
//  struct AST_BLOCK treeBlock = start->data.AST_BLOCK;

  // Shove the entire function into a single block
  traverseStmt(block, start);

  // Split block into basic blocks

  return block;
}

static TAC_FUNCTION traverseFunction(AST* ptr) {
  TAC_FUNCTION function;
  tempNo = 0;
  labelNo = 0;
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_ISR:
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        function.pure = TAC_PURITY_UNKNOWN;
        function.scopeIndex = ast.scopeIndex;
        function.module = SYMBOL_TABLE_getNameFromStart(ast.scopeIndex);
        function.name = data.identifier;
        function.used = false;
        function.start = TAC_generateBasicBlocks(data.body);
        return function;
      }
    default: break;
  }
  return (TAC_FUNCTION){};
}

static TAC_DATA traverseGlobal(AST* global) {
  TAC_DATA tacData = (TAC_DATA){};
  AST ast = *global;
  switch (ast.tag) {
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        tacData.module = SYMBOL_TABLE_getNameFromStart(ast.scopeIndex);
        tacData.name = data.identifier;
        tacData.type = data.type->type;
        tacData.constant = true;
      }
      break;
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        tacData.module = SYMBOL_TABLE_getNameFromStart(ast.scopeIndex);
        tacData.name = data.identifier;
        tacData.type = data.type->type;
        tacData.constant = false;
      }
      break;
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        tacData.module = SYMBOL_TABLE_getNameFromStart(ast.scopeIndex);
        tacData.name = data.identifier;
        tacData.type = data.type->type;
        tacData.constant = false;
      }
      break;
    default:
      break;
  }
  return tacData;
}

TAC_PROGRAM emitTAC(AST* ptr) {
  TAC_PROGRAM program = { NULL };
  struct SECTION* sections = prepareTree(ptr);

  /* do stuff */
  for (int i = 0; i < arrlen(sections); i++) {
    struct SECTION treeSection = sections[i];
    TAC_SECTION section = { i, EMPTY_STRING, NULL, NULL };
    section.name = treeSection.name;
    for (int j = 0; j < arrlen(treeSection.globals); j++) {
      TAC_DATA data = traverseGlobal(treeSection.globals[j]);
      arrput(section.data, data);
    }

    for (int j = 0; j < arrlen(treeSection.functions); j++) {
      TAC_FUNCTION function = traverseFunction(treeSection.functions[j]);
      arrput(section.functions, function);
    }
    arrput(program.sections, section);
  }

  for (int i = 0; i < arrlen(sections); i++) {
    arrfree(sections[i].globals);
    arrfree(sections[i].functions);
  }
  arrfree(sections);
  return program;
}

void emitProgram(TAC_PROGRAM program, PLATFORM p) {

}

void TAC_free(TAC_PROGRAM program) {
  for (int i = 0; i < arrlen(program.sections); i++) {
    TAC_SECTION section = program.sections[i];
    arrfree(program.sections[i].data);
    for (int j = 0; j < arrlen(section.functions); j++) {
      TAC_BLOCK* block = section.functions[j].start;
      TAC* current = block->start;
      while (current != NULL) {
        TAC* next = current->next;
        free(current);
        current = next;
      }
      free(block);
      // TODO: free the whole linked block structure
    }
    arrfree(program.sections[i].functions);
  }
  arrfree(program.sections);
}

static void dumpOperand(TAC_OPERAND operand) {
  switch (operand.tag) {
    case TAC_OPERAND_LITERAL:
      {
        printValue(operand.data.TAC_OPERAND_LITERAL.value);
        break;
      }
    case TAC_OPERAND_VARIABLE:
      {
        printf("%s", CHARS(operand.data.TAC_OPERAND_VARIABLE.name));
        break;
      }
    case TAC_OPERAND_TEMPORARY:
      {
        printf("t%i", operand.data.TAC_OPERAND_TEMPORARY.n);
        break;
      }
    case TAC_OPERAND_LABEL:
      {
        printf("L%i", operand.data.TAC_OPERAND_LABEL.n);
        break;
      }
    default:
      break;
  }
}

static void dumpOperator(TAC_OP_TYPE op) {
  switch (op) {
    case TAC_OP_ERROR: printf(" ????? "); break;
    case TAC_OP_NONE: printf(""); break;
    case TAC_OP_ADD: printf("+"); break;
    case TAC_OP_SUB: printf("-"); break;
    case TAC_OP_MUL: printf("*"); break;
    case TAC_OP_DIV: printf("/"); break;
    case TAC_OP_MOD: printf("%%"); break;
    case TAC_OP_AND: printf("&&"); break;
    case TAC_OP_OR: printf("||"); break;
    case TAC_OP_LSR: printf(">>"); break;
    case TAC_OP_LSL: printf("<<"); break;
    case TAC_OP_GREATER: printf(">"); break;
    case TAC_OP_LESS: printf("<"); break;
    case TAC_OP_GREATER_EQUAL: printf(">="); break;
    case TAC_OP_LESS_EQUAL: printf("<="); break;
    case TAC_OP_EQUAL: printf("=="); break;
    case TAC_OP_NOT_EQUAL: printf("!="); break;
    case TAC_OP_BITWISE_AND: printf("&"); break;
    case TAC_OP_BITWISE_OR: printf("|"); break;
    case TAC_OP_BITWISE_XOR: printf("^"); break;
    case TAC_OP_BITWISE_NOT: printf("~"); break;
    case TAC_OP_NEG: printf("-"); break;
    case TAC_OP_NOT: printf("!"); break;
  }
}

void TAC_dump(TAC_PROGRAM program) {
  for (int i = 0; i < arrlen(program.sections); i++) {
    TAC_SECTION section = program.sections[i];
    printf("Section %i - %s\n", section.index, CHARS(section.name));
    for (int j = 0; j < arrlen(section.data); j++) {
      TAC_DATA data = section.data[j];
      if (data.module != EMPTY_STRING) {
        printf("%s::", CHARS(data.module));
      }

      printf("%s :  %s - %s\n", CHARS(data.name), CHARS(TYPE_get(data.type).name), data.constant ? "constant" : "variable");
    }
    for (int j = 0; j < arrlen(section.functions); j++) {
      TAC_FUNCTION fn = section.functions[j];
      printf("fn ");
      if (fn.module != EMPTY_STRING) {
        printf("%s::", CHARS(fn.module));
      }
      printf("%s(...)\n", CHARS(fn.name));

      TAC_BLOCK* block = fn.start;
      while (block != NULL) {
        TAC* instruction = block->start;
        while (instruction != NULL) {
          switch (instruction->tag) {
            case TAC_TYPE_COPY:
              {
                printf("COPY ");
                dumpOperand(instruction->op1);
                printf(" <-- ");
                if (instruction->op == TAC_OP_NONE) {
                  dumpOperand(instruction->op2);
                } else if (instruction->op == TAC_OP_NEG) {
                  dumpOperator(instruction->op);
                  dumpOperand(instruction->op2);
                } else if (instruction->op == TAC_OP_BITWISE_NOT) {
                  dumpOperator(instruction->op);
                  dumpOperand(instruction->op2);
                } else if (instruction->op == TAC_OP_NOT) {
                  dumpOperator(instruction->op);
                  dumpOperand(instruction->op2);
                } else {
                  printf("(");
                  dumpOperand(instruction->op2);
                  printf(" ");
                  dumpOperator(instruction->op);
                  printf(" ");
                  dumpOperand(instruction->op3);
                  printf(")");
                }
                printf("\n");
                break;
              }
            case TAC_TYPE_GOTO:
              {
                printf("GOTO ");
                dumpOperand(instruction->op1);
                printf("\n");
                break;
              }
            case TAC_TYPE_IF:
              {
                printf("IF_FALSE ");
                dumpOperand(instruction->op1);
                printf(" -> ");
                dumpOperand(instruction->op2);
                printf("\n");
                break;
              }
            case TAC_TYPE_LABEL:
              {
                dumpOperand(instruction->op1);
                printf(":\n");
                break;
              }
            default:
              printf("instruction\n");
          }
          instruction = instruction->next;
        }
        block = block->next;
      }
    }
  }
}
