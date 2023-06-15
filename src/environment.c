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

#include "environment.h"

bool assign(Environment* env, STR name, Value value) {
  if (env == NULL) {
    return false;
  }

  if (hmgeti(env->values, name) == -1) {
    if (env->enclosing != NULL) {
      return assign(env->enclosing, name, value);
    }

    printf("Cannot assign to undefined variable %s.\n", CHARS(name));
    return false;
  }

  if (hmget(env->values, name).constant) {
    printf("Cannot reassign a constant value.\n");
    return false;
  }
  hmput(env->values, name, ((ENV_ENTRY){ value, false }));
  return true;
}

bool define(Environment* env, STR name, Value value, bool constant) {
  if (hmgeti(env->values, name) != -1 && shget(env->values, name).constant) {
    printf("Cannot reassign.\n");
    return false;
  }

  hmput(env->values, name, ((ENV_ENTRY){ value, constant }));
  return true;
}

Value getSymbol(Environment* env, STR name) {
  if (env == NULL) {
    printf("shouldn't get here\n");
    return ERROR(2);
  }
  if (hmgeti(env->values, name) == -1) {
    if (env->enclosing != NULL) {
      return getSymbol(env->enclosing, name);
    }

    printf("Cannot read from undefined variable: %s.\n", CHARS(name));
    return ERROR(1);
  }

  return hmget(env->values, name).value;
}

Environment beginScope(Environment* env) {
  return (Environment){ env, NULL };
}

void endScope(Environment* env) {
  shfree(env->values);
}
