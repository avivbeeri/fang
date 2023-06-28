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
#include "ast.h"
#include "scanner.h"
#include "parser.h"
#include "type_table.h"
#include "const_table.h"
#include "symbol_table.h"
#include "resolve.h"
#include "print.h"
#include "dump.h"
#include "emit.h"
#include "tac.h"
#include "eval.h"
#include "options.h"
#include "platform.h"

bool compile(const SourceFile* sources) {

  if (options.scanTest) {
    testScanner(sources);
  }
  TYPE_TABLE_init();
  CONST_TABLE_init();
  initScanner(sources);
  AST* ast = parse(sources);
  bool result = true;
  if (ast == NULL) {
    result = false;
    goto cleanup;
  }
  if (options.printAst) {
    printTree(ast);
  }

  PLATFORM_init();
  PLATFORM p = PLATFORM_get("apple_arm64");

  result &= resolveTree(ast);
  if (!result) {
    result = false;
    goto cleanup;
  }
  result &= p.calculateSizes();
  if (options.report) {
    p.reportTypeTable();
  }
  if (!result) {
    goto cleanup;
  }

  SYMBOL_TABLE_calculateAllocations(p);
  if (options.report) {
    SYMBOL_TABLE_report();
  }

  if (options.dumpAst) {
    dumpTree(ast);
  }

  if (result) {
    emitTree(ast, p);
    /*
    TAC_PROGRAM program = emitTAC(ast);
    emitProgram(program, p);
    TAC_dump(program);
    */
    // evalTree(ast);
  }
cleanup:
  if (ast != NULL) {
    AST_free(ast);
  }

  CONST_TABLE_free();
  TYPE_TABLE_free();
  SYMBOL_TABLE_free();
  return result;
}
