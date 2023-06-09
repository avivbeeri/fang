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
#include <string.h>
#include <sys/time.h>
#include "common.h"
#include "memory.h"
#include "compiler.h"
#include "options.h"


void OPTIONS_init(void) {
  options.toTerminal = false;
  options.report = false;
  options.scanTest = false;
  options.printAst = false;
  options.dumpAst = false;
  options.timeRun = false;
  options.outfile = NULL;
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int main(int argc, const char* argv[]) {
  struct timeval t1, t2;
  double elapsedTime;

  OPTIONS_init();
  STR_init();

  // start timer
  gettimeofday(&t1, NULL);

  char* path = "example.fg";
  if (argc > 1) {
    path = (char*)argv[1];
  }
  if (argc > 2) {
    options.outfile = (char*)argv[2];
  }

  char* fileSource = readFile(path);
  SourceFile* sources = NULL;
  arrput(sources, ((SourceFile){ .name=path, .source = fileSource}));

  bool success = compile(sources);
  for (int i = 0; i < arrlen(sources); i++) {
    free((void*)sources[i].source);
  }
  arrfree(sources);

  gettimeofday(&t2, NULL);
  elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
  elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms

  if (options.timeRun) {
    printf("Completed in %f milliseconds.\n", elapsedTime);
  }
  if (success) {
    printf("OK\n");
  } else {
    printf("Fail\n");
  }
  STR_free();
  return success ? 0 : 1;
}
