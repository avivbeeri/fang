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
#include "eval.h"

bool compile(const char* source) {
  // testScanner(source);
  TYPE_TABLE_init();
  CONST_TABLE_init();
  initScanner(source);
  AST* ast = parse(source);
  bool result = true;
  if (ast != NULL) {
    printTree(ast);
    if (resolveTree(ast)) {
      printf("Resolved successfully.\n");
      // dumpTree(ast);
      emitTree(ast);
      // evalTree(ast);
    } else {
      printf("Failed to compile program.\n");
      result = false;
    }
    ast_free(ast);
  } else {
    result = false;
  }
  CONST_TABLE_free();
  TYPE_TABLE_free();
  SYMBOL_TABLE_free();
  printf("\n");
  return result;
}
