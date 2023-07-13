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

static int traverseStmt(void* context, AST* ptr) {

}

static TAC_BLOCK* TAC_generateBasicBlocks(AST* start) {
  TAC_BLOCK* block = calloc(1, sizeof(TAC_BLOCK));
  struct AST_BLOCK treeBlock = start.data.AST_BLOCK;

  // Shove the entire function into a single block
  traverseStmt(block, treeBlock.decls[i]);

  // Split block into basic blocks

  return block;
}

static TAC_FUNCTION traverseFunction(AST* ptr) {
  TAC_FUNCTION function;
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
      }
    default:
  }
  return function;
}

static TAC_DATA traverseGlobal(AST* global) {
  TAC_DATA tacData;
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
      // TODO: free the whole linked structure
      free(section.functions[j].start);
    }
    arrfree(program.sections[i].functions);
  }
  arrfree(program.sections);
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
    }
  }
}
