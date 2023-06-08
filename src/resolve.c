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
#include "common.h"
#include "parser.h"
#include "ast.h"
#include "error.h"
#include "print.h"
#include "type_table.h"
#include "symbol_table.h"
#include "options.h"
#include "const_eval.h"

uint32_t* assignStack = NULL;
uint32_t* evaluateStack = NULL;
uint32_t* typeStack = NULL;
uint32_t* kindStack = NULL;
bool functionScope = false;

#define VOID_INDEX 1
#define BOOL_INDEX 2
#define U8_INDEX 3
#define I8_INDEX 4
#define U16_INDEX 5
#define I16_INDEX 6
#define NUMERICAL_INDEX 7
#define STRING_INDEX 8
#define FN_INDEX 9
#define CHAR_INDEX 10

static bool isLiteral(int type) {
  return type == NUMERICAL_INDEX;
}
static void printKind(int kind) {
  switch (kind) {
    case SYMBOL_KIND_SCALAR: printf("scalar"); break;
    case SYMBOL_KIND_POINTER: printf("pointer"); break;
    case SYMBOL_KIND_RECORD: printf("record"); break;
    case SYMBOL_KIND_ARRAY: printf("array"); break;
    case SYMBOL_KIND_FUNCTION: printf("fun"); break;
  }
}

static bool isPointer(int type) {
  return typeTable[type].entryType == ENTRY_TYPE_POINTER || typeTable[type].entryType == ENTRY_TYPE_ARRAY || type == STRING_INDEX;
}
static bool isNumeric(int type) {
  return (type <= NUMERICAL_INDEX && type > BOOL_INDEX) || isPointer(type);
}

static bool isCompatible(int type1, int type2) {
  return type1 == type2
    || (isNumeric(type1) && isLiteral(type2))
    || (isLiteral(type1) && isNumeric(type2))
    || (isLiteral(type1) && isLiteral(type2))
    || (type1 == STRING_INDEX && type2 == TYPE_TABLE_lookup("^char"))
    || (type2 == STRING_INDEX && type1 == TYPE_TABLE_lookup("^char"))
    || (isPointer(type1) && isPointer(type2) && typeTable[type1].parent == typeTable[type2].parent);
}
static int coerceType(int type1, int type2) {
  if (type1 == type2) {
    return type1;
  }
  if (isNumeric(type1) && isLiteral(type2)) {
    return type1;
  }
  if (isLiteral(type1) && isNumeric(type2)) {
    return type2;
  }
  /*
  if (isNumeric(type1) && isNumeric(type2)) {
    return typeTable[type1].byteSize > typeTable[type2].byteSize ? type1 : type2;
  }
  */
  if (isPointer(type1)) {
    return type1;
  }
  if (isPointer(type2)) {
    return type2;
  }

  return NUMERICAL_INDEX;
}

static int valueToType(Value value) {
  switch (value.type) {
    case VAL_ERROR: return 0;
    case VAL_UNDEF: return 1; // void
    case VAL_BOOL: return BOOL_INDEX;

    case VAL_CHAR: return CHAR_INDEX;
    case VAL_U8: return U8_INDEX;

    case VAL_I8: return I8_INDEX;

    case VAL_U16: return U16_INDEX;
    case VAL_PTR: return STRING_INDEX;

    case VAL_I16: return I16_INDEX;
    case VAL_LIT_NUM: return NUMERICAL_INDEX;
    case VAL_STRING: return STRING_INDEX;
    case VAL_RECORD: return 0;
    case VAL_ARRAY: return 0;
    // Open to implicit num casting
  }
  return 0;
}

static SYMBOL_KIND getSymbolKind(int typeIndex) {
  if (typeTable[typeIndex].entryType == ENTRY_TYPE_FUNCTION) {
    return SYMBOL_KIND_FUNCTION;
  }
  if (typeTable[typeIndex].entryType == ENTRY_TYPE_ARRAY) {
    return SYMBOL_KIND_ARRAY;
  }
  if (typeTable[typeIndex].entryType == ENTRY_TYPE_RECORD) {
    return SYMBOL_KIND_RECORD;
  }
  if (typeTable[typeIndex].entryType == ENTRY_TYPE_POINTER) {
    return SYMBOL_KIND_POINTER;
  }
  return SYMBOL_KIND_SCALAR;
}

static bool traverse(AST* ptr);
static int resolveType(AST* ptr) {
  if (ptr == NULL) {
    return 0;
  }
  AST ast = *ptr;
  switch (ast.tag) {
    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        int i = resolveType(data.type);
        ptr->type = i;
        return i;
      }
    case AST_TYPE_NAME:
      {
        struct AST_TYPE_NAME data = ast.data.AST_TYPE_NAME;
        int i = TYPE_TABLE_lookupWithString(data.typeName);
        ptr->type = i;
        return i;
      }
    case AST_TYPE_PTR:
      {
        struct AST_TYPE_PTR data = ast.data.AST_TYPE_PTR;
        int subType = resolveType(data.subType);
        // Get type name from typetable
        // prepend ^
        // store in type table and record index
        STRING* name = typeTable[subType].name;
        STRING* typeName = STRING_prepend(name, "^");
        ptr->type = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, subType, NULL);
        return ptr->type;
      }
    case AST_TYPE_FN:
      {
        struct AST_TYPE_FN data = ast.data.AST_TYPE_FN;
        TYPE_TABLE_FIELD_ENTRY* entries = NULL;
        // generate the fn type string here
        size_t bufLen = 1;
        size_t strLen = 0;
        char* buffer = ALLOC_STR(bufLen);
        APPEND_STR(buffer, bufLen, strLen, "fn (");
        for (int i = 0; i < arrlen(data.params); i++) {
          int index = resolveType(data.params[i]);
          APPEND_STR(buffer, bufLen, strLen, "%s", typeTable[index].name->chars);
          arrput(entries, ((TYPE_TABLE_FIELD_ENTRY){ NULL, index }));
          if (i < arrlen(data.params) - 1) {
            APPEND_STR(buffer, bufLen, strLen, ", ");
          }
        }
        APPEND_STR(buffer, bufLen, strLen, "): ");
        int returnType = resolveType(data.returnType);
        APPEND_STR(buffer, bufLen, strLen, "%s", typeTable[returnType].name->chars);

        STRING* typeName = copyString(buffer, strLen);
        FREE(char, buffer);
        ptr->type = TYPE_TABLE_declare(typeName);
        TYPE_TABLE_defineCallable(ptr->type, FN_INDEX, entries, returnType);
        return ptr->type;
      }
    case AST_TYPE_ARRAY:
      {
        // Arrays are basically just pointers with some runtime allocation
        // semantics
        struct AST_TYPE_ARRAY data = ast.data.AST_TYPE_ARRAY;
        if (data.length != NULL) {
          traverse(data.length);
          int lenType = data.length->type;
          if (!isNumeric(lenType)) {
            return 0;
          }
        }

        int subType = resolveType(data.subType);
        STRING* name = typeTable[subType].name;

        STRING* ptrName = STRING_prepend(name, "^");
        TYPE_TABLE_registerType(ptrName, ENTRY_TYPE_POINTER, subType, NULL);
        STRING* typeName = STRING_prepend(name, "[]");
        ptr->type = TYPE_TABLE_registerArray(typeName, ENTRY_TYPE_ARRAY, subType);
        return ptr->type;
      }
  }
  return 0;
}

static bool resolveTopLevel(AST* ptr) {
  if (ptr == NULL) {
    return true;
  }
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        return false;
      }
    case AST_MAIN:
      {
        struct AST_MAIN data = ast.data.AST_MAIN;
        bool r = true;
        for (int i = 0; i < arrlen(data.modules); i++) {
          SYMBOL_TABLE_openScope(SCOPE_TYPE_MODULE);
          r &= resolveTopLevel(data.modules[i]);
          SYMBOL_TABLE_closeScope();
          if (!r) {
            return r;
          }
        }
        return true;
      }
    case AST_MODULE_DECL:
      {
        struct AST_MODULE_DECL data = ast.data.AST_MODULE_DECL;
        bool result = SYMBOL_TABLE_nameScope(data.name);
        if (!result) {
          compileError(ast.token, "module \"%s\" is already defined.\n", data.name->chars);
        }
        return result;
      }
    case AST_EXT:
      {
        struct AST_EXT data = ast.data.AST_EXT;
        int resolvedType = resolveType(data.type);
        if (data.symbolType == SYMBOL_TYPE_FUNCTION) {
          SYMBOL_TABLE_declareWithKind(data.identifier, data.symbolType, SYMBOL_KIND_FUNCTION, resolvedType, STORAGE_TYPE_GLOBAL);
        } else {
          SYMBOL_KIND kind = getSymbolKind(resolvedType);

          if (data.symbolType == SYMBOL_TYPE_CONSTANT) {
            SYMBOL_TABLE_declareWithKind(data.identifier, data.symbolType, kind, resolvedType, STORAGE_TYPE_GLOBAL);
          } else if (data.symbolType == SYMBOL_TYPE_VARIABLE) {
            SYMBOL_TABLE_declareWithKind(data.identifier, data.symbolType, kind, resolvedType, STORAGE_TYPE_GLOBAL);
          }
        }
        return true;
      }
    case AST_MODULE:
      {
        bool r = true;
        struct AST_MODULE data = ast.data.AST_MODULE;
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        int* deferred = NULL;
        for (int i = 0; i < arrlen(data.decls); i++) {
          if (data.decls[i]->tag == AST_FN) {
            // FN pointer type resolution needs to occur after other types
            // are resolved.
            arrput(deferred, i);
            continue;
          }
          r = resolveTopLevel(data.decls[i]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }

        for (int i = 0; i < arrlen(deferred); i++) {
          r &= resolveTopLevel(data.decls[deferred[i]]);
          if (!r) {
            break;
          }
        }
        arrfree(deferred);
        return r;
      }
    case AST_TYPE_DECL:
      {
        struct AST_TYPE_DECL data = ast.data.AST_TYPE_DECL;
        STRING* identifier = data.name;
        int index = TYPE_TABLE_declare(identifier);
        TYPE_TABLE_FIELD_ENTRY* fields = NULL;
        // should we resolve type fields before main verification?

        for (int i = 0; i < arrlen(data.fields); i++) {
          struct AST_PARAM field = data.fields[i]->data.AST_PARAM;
          traverse(field.value);
          int index = field.value->type;
          if (index == 0) {
            arrfree(fields);
            return false;
          }
          SYMBOL_KIND kind = getSymbolKind(index);
          int elementCount = 0;
          if (typeTable[index].entryType == ENTRY_TYPE_ARRAY) {
            int subType = typeTable[index].parent;
            STRING* name = typeTable[subType].name;
            STRING* typeName = STRING_prepend(name, "[]");
            index = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_ARRAY, subType, NULL);
            if (field.value->tag == AST_TYPE) {
              Value length = evalConstTree(field.value);
              if (!IS_EMPTY(length) && !IS_ERROR(length)) {
                elementCount = getNumber(length);
              }
            }
          }
          arrput(fields, ((TYPE_TABLE_FIELD_ENTRY){
                .typeIndex = index,
                .name = field.identifier,
                .elementCount = elementCount,
                .kind = kind
          } ));
        }
        TYPE_TABLE_define(index, ENTRY_TYPE_RECORD, 0, fields);
        return true;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        ptr->type = resolveType(data.fnType);
        if (SYMBOL_TABLE_getCurrent(data.identifier).defined) {
          compileError(ast.token, "function \"%s\" is already defined.\n", data.identifier->chars);
          return false;
        }
        SYMBOL_TABLE_define(data.identifier, SYMBOL_TYPE_FUNCTION, SYMBOL_KIND_FUNCTION, ptr->type, STORAGE_TYPE_GLOBAL);
        return true;
      }
    default:
      {
        return true;
      }
  }
  return false;
}

static bool traverse(AST* ptr) {
  if (ptr == NULL) {
    return true;
  }
  ptr->rvalue = PEEK(evaluateStack);
  AST ast = *ptr;
  switch(ast.tag) {
    case AST_ERROR:
      {
        return false;
      }
    case AST_MAIN:
      {
        struct AST_MAIN data = ast.data.AST_MAIN;
        bool r = true;
        for (int i = 0; i < arrlen(data.modules); i++) {
          SYMBOL_TABLE_pushScope(data.modules[i]->scopeIndex);
          r &= traverse(data.modules[i]);
          SYMBOL_TABLE_closeScope();
          if (!r) {
            return r;
          }
        }
        return r;
      }
    case AST_RETURN:
      {
        struct AST_RETURN data = ast.data.AST_RETURN;
        if (data.value == NULL) {
          return PEEK(typeStack) == VOID_INDEX;
        }
        bool r = traverse(data.value);
        if (!r) {
          return r;
        }
        if (!isCompatible(data.value->type, PEEK(typeStack))) {
          printf("MISMATCH between return type and expecte\n");
          int indent = compileError(data.value->token, "Incompatible return type '");
          printTree(data.value);
          printf("'\n");
          printf("%*s", indent, "");
          printf("Expected type '%s' but instead found '%s'\n", typeTable[PEEK(typeStack)].name->chars, typeTable[data.value->type].name->chars);
          return false;
        }
        return r;
      }
    case AST_BLOCK:
      {
        struct AST_BLOCK data = ast.data.AST_BLOCK;
        SYMBOL_TABLE_openScope(SCOPE_TYPE_BLOCK);
        bool r = true;
        for (int i = 0; i < arrlen(data.decls); i++) {
          r &= traverse(data.decls[i]);
          if (!r) {
            break;
          }
        }
        SYMBOL_TABLE_closeScope();
        return r;
      }
    case AST_MODULE:
      {
        bool r = true;
        struct AST_MODULE data = ast.data.AST_MODULE;
        int* deferred = NULL;
        for (int i = 0; i < arrlen(data.decls); i++) {
          // Hoist FN resolution until after the main code
          // for type-check reasons
          if (data.decls[i]->tag == AST_FN) {
            arrput(deferred, i);
            continue;
          }
          r = traverse(data.decls[i]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }

        for (int i = 0; i < arrlen(deferred); i++) {
          r = traverse(data.decls[deferred[i]]);
          if (!r) {
            arrfree(deferred);
            return false;
          }
        }
        arrfree(deferred);
        return r;
      }
    case AST_VAR_INIT:
      {
        struct AST_VAR_INIT data = ast.data.AST_VAR_INIT;
        STRING* identifier = data.identifier;
        bool r  = traverse(data.type);
        int leftType = data.type->type;
        SYMBOL_KIND kind = getSymbolKind(leftType);
        PUSH(kindStack, kind);
        PUSH(typeStack, leftType);
        PUSH(evaluateStack, true);
        r &= traverse(data.expr);
        POP(evaluateStack);
        POP(typeStack);
        POP(kindStack);
        int rightType = data.expr->type;
        if (!r) {
          return false;
        }

        bool result = r && isCompatible(leftType, rightType);
        if ((typeTable[leftType].entryType == ENTRY_TYPE_ARRAY || typeTable[leftType].entryType == ENTRY_TYPE_RECORD)
            && leftType != rightType) {

          if (leftType == STRING_INDEX && data.expr->type == STRING_INDEX) {

          } else {
            char* initType = data.expr->data.AST_INITIALIZER.initType == INIT_TYPE_ARRAY ? "array" : "record";
            compileError(ast.token, "Attempting to initialize %s using literal '", typeTable[leftType].name->chars);
            printTree(data.expr);
            printf("'.\n");
            return false;
          }
        }
        if (!isCompatible(leftType, rightType)) {
          int indent = compileError(data.expr->token, "Incompatible initialization for variable '%s'", data.identifier->chars);
          printf("%*s", indent, "");
          printf("Expected type '%s' but instead found '%s'\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
          return false;
        }
        if (SYMBOL_TABLE_getCurrentOnly(identifier).defined) {
          compileError(ast.token, "variable \"%s\" is already defined.\n", data.identifier->chars);
          return false;
        }
        SYMBOL_TABLE_STORAGE_TYPE storageType = functionScope ? STORAGE_TYPE_LOCAL : STORAGE_TYPE_GLOBAL;
        int index = leftType;
        if (typeTable[leftType].entryType == ENTRY_TYPE_ARRAY) {
          int subType = typeTable[leftType].parent;
          STRING* name = typeTable[subType].name;
          STRING* typeName = STRING_prepend(name, "^");
          index = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, subType, NULL);
        }
        if (typeTable[leftType].entryType == ENTRY_TYPE_ARRAY || typeTable[leftType].entryType == ENTRY_TYPE_RECORD) {
          storageType = functionScope ? STORAGE_TYPE_LOCAL_OBJECT : STORAGE_TYPE_GLOBAL_OBJECT;
        }
        SYMBOL_TABLE_define(identifier, SYMBOL_TYPE_VARIABLE, kind, index, storageType);
        int elementCount = 0;
        if (kind == SYMBOL_KIND_ARRAY) {
          Value length = evalConstTree(data.type);
          if (!IS_EMPTY(length) && !IS_ERROR(length)) {
            elementCount = getNumber(length);
            printf("Array length: %i\n", elementCount);
            printf("%i\n", elementCount);
            SYMBOL_TABLE_updateElementCount(identifier, elementCount);
          }
        }
        ptr->type = leftType;
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        return result;
      }
    case AST_VAR_DECL:
      {
        struct AST_VAR_DECL data = ast.data.AST_VAR_DECL;
        STRING* identifier = data.identifier;
        bool r = traverse(data.type);
        int typeIndex = data.type->type;
        if (typeIndex == STRING_INDEX) {
          return false;
        }
        ptr->type = typeIndex;
        if (SYMBOL_TABLE_getCurrentOnly(identifier).defined) {
          compileError(ast.token, "variable \"%s\" is already defined.\n", data.identifier->chars);
          return false;
        }
        SYMBOL_KIND kind = getSymbolKind(typeIndex);
        SYMBOL_TABLE_STORAGE_TYPE storageType = functionScope ? STORAGE_TYPE_LOCAL : STORAGE_TYPE_GLOBAL;
        if (typeTable[typeIndex].entryType == ENTRY_TYPE_ARRAY || typeTable[typeIndex].entryType == ENTRY_TYPE_RECORD) {
          storageType = functionScope ? STORAGE_TYPE_LOCAL_OBJECT : STORAGE_TYPE_GLOBAL_OBJECT;
        }
        SYMBOL_TABLE_define(identifier, SYMBOL_TYPE_VARIABLE, kind, typeIndex, storageType);
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        return r;
      }
    case AST_CONST_DECL:
      {
        struct AST_CONST_DECL data = ast.data.AST_CONST_DECL;
        STRING* identifier = data.identifier;
        bool r = traverse(data.type);
        int leftType = data.type->type;
        SYMBOL_KIND kind = getSymbolKind(leftType);
        PUSH(kindStack, kind);
        PUSH(typeStack, leftType);
        PUSH(evaluateStack, true);
        r &= traverse(data.expr);
        POP(evaluateStack);
        POP(kindStack);
        int rightType = data.expr->type;
        POP(typeStack);

        bool result = r && isCompatible(leftType, rightType);
        if (!isCompatible(leftType, rightType)) {
          int indent = compileError(data.expr->token, "Incompatible initialization for constant value '%s'", data.identifier->chars);
          printf("%*s", indent, "");
          printf("Expected type '%s' but instead found '%s'\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
          return false;
        }
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        SYMBOL_TABLE_STORAGE_TYPE storageType = functionScope ? STORAGE_TYPE_LOCAL : STORAGE_TYPE_GLOBAL;
        if (SYMBOL_TABLE_getCurrentOnly(identifier).defined) {
          compileError(ast.token, "constant \"%s\" is already defined.\n", data.identifier->chars);
          return false;
        }
        if (typeTable[leftType].entryType == ENTRY_TYPE_ARRAY || typeTable[leftType].entryType == ENTRY_TYPE_RECORD) {
          storageType = functionScope ? STORAGE_TYPE_LOCAL_OBJECT : STORAGE_TYPE_GLOBAL_OBJECT;
        }
        if (typeTable[leftType].entryType == ENTRY_TYPE_ARRAY) {
          Value length = evalConstTree(data.type);

          int subType = typeTable[leftType].parent;
          STRING* name = typeTable[subType].name;
          STRING* typeName = STRING_prepend(name, "^");
          TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, subType, NULL);
        }
        ptr->type = leftType;
        if (ptr->scopeIndex <= 1 || ast.tag == AST_CONST_DECL) {
          SYMBOL_TABLE_define(identifier, SYMBOL_TYPE_CONSTANT, kind, leftType, storageType);
        } else {
          SYMBOL_TABLE_define(identifier, SYMBOL_TYPE_VARIABLE, kind, leftType, storageType);
        }
        int elementCount = 0;
        if (kind == SYMBOL_KIND_ARRAY) {
          Value length = evalConstTree(data.type);
          if (!IS_EMPTY(length) && !IS_ERROR(length)) {
            elementCount = getNumber(length);
            printf("Array length: %i\n", elementCount);
            printf("%i\n", elementCount);
            SYMBOL_TABLE_updateElementCount(identifier, elementCount);
          }
        }
        return result;
      }
    case AST_ASSIGNMENT:
      {
        struct AST_ASSIGNMENT data = ast.data.AST_ASSIGNMENT;

        if (data.lvalue->tag == AST_IDENTIFIER) {
          SYMBOL_TABLE_ENTRY entry = SYMBOL_TABLE_getCurrent(data.lvalue->data.AST_IDENTIFIER.identifier);
          if (entry.defined && entry.entryType == SYMBOL_TYPE_CONSTANT) {
            compileError(ast.token, "attempting to assign to read-only constant \"%s\".\n", data.lvalue->data.AST_IDENTIFIER.identifier->chars);
            return false;
          }
        }

        PUSH(assignStack, true);
        PUSH(evaluateStack, false);
        bool ident = traverse(data.lvalue);
        POP(evaluateStack);
        POP(assignStack);
        int leftType = data.lvalue->type;

        PUSH(typeStack, leftType);
        PUSH(evaluateStack, true);
        bool expr = traverse(data.expr);
        POP(evaluateStack);
        POP(typeStack);
        int rightType = data.expr->type;

        if (!(ident && expr)) {
          printf("trap %d\n", __LINE__);
          return false;
        }
        ptr->type = leftType;
        if (!isCompatible(leftType, rightType)) {
          int indent = compileError(data.expr->token, "Incompatible assignment for variable '");
          printTree(data.lvalue);
          printf("'\n");
          printf("%*s", indent, "");
          printf("Expected type '%s' but instead found '%s'\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
        }
        return isCompatible(leftType, rightType);
      }
    case AST_ASM:
      {
        ptr->type = 1;
        return true;
      }
    case AST_CAST:
      {
        struct AST_CAST data = ast.data.AST_CAST;
        bool r = traverse(data.expr) && traverse(data.type);
        ptr->type = data.type->type;
        return r;
      }
    case AST_TYPE:
      {
        struct AST_TYPE data = ast.data.AST_TYPE;
        ptr->type = resolveType(data.type);
        return ptr->type != 0;
      }
    case AST_FN:
      {
        struct AST_FN data = ast.data.AST_FN;
        // Define symbol with parameter types
        bool r = true;

        SYMBOL_TABLE_openScope(SCOPE_TYPE_FUNCTION);
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        for (int i = 0; i < arrlen(data.params); i++) {
          struct AST_PARAM param = data.params[i]->data.AST_PARAM;
          STRING* paramName = param.identifier;
          uint32_t index = typeTable[ast.type].fields[i].typeIndex;
          SYMBOL_KIND kind = SYMBOL_KIND_SCALAR;
          SYMBOL_TABLE_define(paramName, SYMBOL_TYPE_PARAMETER, kind, index, STORAGE_TYPE_PARAMETER);
        }
        PUSH(typeStack, typeTable[ast.type].returnType);
        functionScope = true;
        r &= traverse(data.body);
        functionScope = false;
        POP(typeStack);
        SYMBOL_TABLE_closeScope();
        return r;
      }
    case AST_LITERAL:
      {
        struct AST_LITERAL data = ast.data.AST_LITERAL;
        Value value = data.value; //CONST_TABLE_get(data.constantIndex);
        int leftType = PEEK(typeStack);
        int rightType = valueToType(value);
        ptr->type = rightType;
        ptr->rvalue = true;
        return true;
      }
    case AST_IDENTIFIER:
      {
        struct AST_IDENTIFIER data = ast.data.AST_IDENTIFIER;
        STRING* identifier = data.identifier;
        bool result;
        SYMBOL_TABLE_ENTRY entry;
        int scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        if (data.module != NULL) {
          scopeIndex = SYMBOL_TABLE_getScopeIndexByName(data.module);
          if (scopeIndex == -1) {
            compileError(ast.token, "identifier '%s' has not yet been defined\n", identifier->chars);
            return false;
          }
          entry = SYMBOL_TABLE_get(scopeIndex, identifier);
        } else {
          entry = SYMBOL_TABLE_getCurrent(identifier);
        }
        if (entry.defined) {
          ptr->scopeIndex = scopeIndex;
          ptr->type = entry.typeIndex;
        } else {
          compileError(ast.token, "identifier '%s' has not yet been defined in this scope.\n", identifier->chars);
          return false;
        }
        if (entry.entryType == SYMBOL_TYPE_CONSTANT && PEEK(assignStack)) {
          compileError(ast.token, "identifier '%s' is a constant and cannot be assigned to.\n", identifier->chars);
          return false;
        }
        return true;
      }
    case AST_INITIALIZER:
      {
        struct AST_INITIALIZER data = ast.data.AST_INITIALIZER;
        TYPE_TABLE_ENTRY entry = typeTable[PEEK(typeStack)];
        ptr->type = PEEK(typeStack);
        if ((data.initType == INIT_TYPE_RECORD && PEEK(kindStack) != SYMBOL_KIND_RECORD)
            || (data.initType == INIT_TYPE_ARRAY && PEEK(kindStack) != SYMBOL_KIND_ARRAY)) {

          char* initType = data.initType == INIT_TYPE_ARRAY ? "array" : "record";
          char* subType;
          if (PEEK(kindStack) == SYMBOL_KIND_ARRAY) {
            subType = typeTable[entry.parent].name->chars;
            compileError(ast.token, "Incompatible %s initializer for an array of '%s'.\n", initType, subType);
          } else if (PEEK(kindStack) == SYMBOL_KIND_RECORD) {
            subType = typeTable[ptr->type].name->chars;
            compileError(ast.token, "Incompatible %s initializer for an record of '%s'.\n", initType, subType);
          } else {
            printf("Impossible initializer type.\n");
            printf("trap %d\n", __LINE__);
            exit(1);
          }
          return false;
        }
        bool r = true;
        if (entry.entryType == ENTRY_TYPE_ARRAY) {
          int subType = typeTable[ptr->type].parent;
          for (int i = 0; i < arrlen(data.assignments); i++) {

            PUSH(typeStack, subType);
            r = traverse(data.assignments[i]);
            POP(typeStack);
            if (!r) {
              printf("trap %d\n", __LINE__);
              return false;
            }
            if (!isCompatible(data.assignments[i]->type, subType)) {
              printf("Initializer doesn't assign correct type %s vs %s\n.", typeTable[data.assignments[i]->type].name->chars, typeTable[subType].name->chars);
              return false;
            }
          }
        } else if (entry.entryType == ENTRY_TYPE_RECORD) {
          for (int i = 0; i < arrlen(data.assignments); i++) {
            struct AST_PARAM field = data.assignments[i]->data.AST_PARAM;
            STRING* name = field.identifier;
            int fieldIndex = 0;
            bool found = false;
            for (; fieldIndex < arrlen(entry.fields); fieldIndex++) {
              if (STRING_equality(name, entry.fields[fieldIndex].name)) {
                found = true;
                break;
              }
            }
            if (!found) {
              r = false;
              printf("trap %d\n", __LINE__);
              compileError(data.assignments[i]->token, "Field '%s' doesn't exist in composite type '%s'\n", name->chars, entry.name->chars);
              return false;
            }
            PUSH(typeStack, entry.fields[fieldIndex].typeIndex);
            PUSH(kindStack, entry.fields[fieldIndex].kind);
            r &= traverse(field.value);
            POP(typeStack);
            POP(kindStack);
            if (!r) {
              return false;
            }
            r &= isCompatible(entry.fields[fieldIndex].typeIndex, field.value->type);
            if (!r) {
              int indent = compileError(data.assignments[i]->token, "Invalid assignment to field '%s' of composite type '%s'.\n", name->chars, entry.name->chars);
              printf("%*s", indent, "");
              printf("You attempted to assign a value of type '%s' to '%s', which are incompatible.\n", typeTable[field.value->type].name->chars, typeTable[entry.fields[fieldIndex].typeIndex].name->chars);
              return false;
            }
            data.assignments[i]->type = entry.fields[fieldIndex].typeIndex;
          }
        }
        return r;
      }
    case AST_REF:
      {
        struct AST_REF data = ast.data.AST_REF;
        bool r = traverse(data.expr);
        int subType = data.expr->type;
        STRING* name = typeTable[subType].name;
        STRING* typeName = STRING_prepend(name, "^");
        ptr->type = TYPE_TABLE_registerType(typeName, ENTRY_TYPE_POINTER, subType, NULL);
        ptr->scopeIndex = SYMBOL_TABLE_getCurrentScopeIndex();
        return r;
      }
    case AST_DEREF:
      {
        struct AST_DEREF data = ast.data.AST_DEREF;
        PUSH(evaluateStack, !PEEK(assignStack));
        ptr->rvalue = PEEK(evaluateStack);
        bool r = traverse(data.expr);
        POP(evaluateStack);
        int subType = data.expr->type;
        TYPE_TABLE_ENTRY entry = typeTable[subType];
        if (entry.parent == 0) {
          printf("trap %d\n", __LINE__);
          return false;
        }
        ptr->type = typeTable[subType].parent;
        if (ptr->type == 0) {
          printf("trap %d\n", __LINE__);
          return false;
        }
        return r;
      }
    case AST_UNARY:
      {
        struct AST_UNARY data = ast.data.AST_UNARY;
        bool r = traverse(data.expr);
        switch (data.op) {
          case OP_BITWISE_NOT:
          case OP_NEG:
            {
              ptr->type = data.expr->type;
              break;
            }
          case OP_NOT:
            {
              ptr->type = BOOL_INDEX; // to bool
              break;
            }
        }
        return r;
      }
    case AST_BINARY:
      {
        struct AST_BINARY data = ast.data.AST_BINARY;
        PUSH(assignStack, false);
        bool r = traverse(data.left);
        int leftType = data.left->type;
        PUSH(typeStack, leftType);
        r &= traverse(data.right);
        POP(typeStack);
        POP(assignStack);
        int rightType = data.right->type;
        if (!r) {
          return false;
        }
        bool compatible = true;

        switch (data.op) {
          // Arithmetic-only operators
          case OP_ADD:
          case OP_SUB:
          case OP_MOD:
          case OP_DIV:
          case OP_MUL:
            {
              if (!isCompatible(leftType, rightType)) {
                compatible = false;
                int indent = compileError(ast.token, "Invalid operands to arithmetic operator '%.*s'\n", ast.token.length, ast.token.start);
                printf("%*s", indent, "");
                printf("Operands were of type '%s' and '%s', which are incompatible.\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
              } else {
                ptr->type = coerceType(leftType, rightType);
              }
              break;
            }

          // Numerical ops
          case OP_SHIFT_LEFT:
          case OP_SHIFT_RIGHT:
          case OP_BITWISE_XOR:
          case OP_BITWISE_AND:
          case OP_BITWISE_OR:
            {
              // Allow numerical types
              if (leftType == rightType) {
                ptr->type = leftType;
              } else if (isNumeric(leftType) && isLiteral(rightType)) {
                ptr->type = leftType;
              } else if (isLiteral(leftType) && isNumeric(rightType)) {
                ptr->type = rightType;
              } else {
                compatible = false;
                int indent = compileError(ast.token, "Invalid operands to bitwise operator '%.*s'\n", ast.token.length, ast.token.start);
                printf("%*s", indent, "");
                printf("Operands were of type '%s' and '%s', which are incompatible.\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
              }
              break;
            }

          // Logical ops, return a bool
          case OP_OR:
          case OP_AND:
            {
              ptr->type = BOOL_INDEX;
              compatible = true;
              break;
            }
          case OP_COMPARE_EQUAL:
          case OP_NOT_EQUAL:
            {
              if (isCompatible(leftType, rightType)) {
                ptr->type = BOOL_INDEX;
              } else {
                // TODO
                printf("Comparison: %s vs %s\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
                compatible = false;
              }
              break;
            }
          case OP_GREATER_EQUAL:
          case OP_LESS_EQUAL:
          case OP_GREATER:
          case OP_LESS:
            {
              if (isCompatible(leftType, rightType)) {
                ptr->type = BOOL_INDEX;
              } else {
                compatible = false;
                // TODO
                printf("Comparison: %s vs %s\n", typeTable[leftType].name->chars, typeTable[rightType].name->chars);
              }
              break;
            }
        }

        return compatible;
      }
    case AST_IF:
      {
        struct AST_IF data = ast.data.AST_IF;
        bool r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        if (data.elseClause != NULL) {
          r = traverse(data.elseClause);
          if (!r) {
            return false;
          }
        }
        return true;
      }
    case AST_DO_WHILE:
      {
        struct AST_DO_WHILE data = ast.data.AST_DO_WHILE;
        bool r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        return true;
      }
    case AST_WHILE:
      {
        struct AST_WHILE data = ast.data.AST_WHILE;
        bool r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        return true;
      }
    case AST_FOR:
      {
        struct AST_FOR data = ast.data.AST_FOR;
        SYMBOL_TABLE_openScope(SCOPE_TYPE_LOOP);
        bool r = traverse(data.initializer);
        if (!r) {
          return false;
        }
        r = traverse(data.condition);
        if (!r) {
          return false;
        }
        r = traverse(data.body);
        if (!r) {
          return false;
        }
        r = traverse(data.increment);
        if (!r) {
          return false;
        }
        SYMBOL_TABLE_closeScope();
        return true;
      }
    case AST_DOT:
      {
        struct AST_DOT data = ast.data.AST_DOT;
        PUSH(assignStack, false);
        PUSH(evaluateStack, true);
        bool r = traverse(data.left);
        POP(evaluateStack);
        POP(assignStack);
        if (!r) {
          return false;
        }
        // Need to pass type upwards to validate field name
        TYPE_TABLE_ENTRY entry = typeTable[data.left->type];
        if (entry.entryType == ENTRY_TYPE_POINTER && typeTable[entry.parent].entryType == ENTRY_TYPE_RECORD) {
          entry = typeTable[entry.parent];
        } else if (entry.entryType != ENTRY_TYPE_RECORD) {
          compileError(ast.token, "Attempting to access field '%s' ", data.name->chars);
          printf("of type '%s' but it is not a record type.\n", typeTable[data.left->type].name->chars);
          return false;
        }
        STRING* name = data.name;
        int fieldIndex = 0;
        bool found = false;
        for (; fieldIndex < arrlen(entry.fields); fieldIndex++) {
          if (STRING_equality(name, entry.fields[fieldIndex].name)) {
            found = true;
            break;
          }
        }
        if (!found) {
          printf("trap %d\n", __LINE__);
          return false;
        }
        ptr->type = entry.fields[fieldIndex].typeIndex;
        return true;
      }
    case AST_SUBSCRIPT:
      {
        struct AST_SUBSCRIPT data = ast.data.AST_SUBSCRIPT;
        PUSH(evaluateStack, true);
        bool r = traverse(data.left);
        POP(evaluateStack);
        if (!r) {
          return false;
        }
        PUSH(assignStack, false);
        r &= traverse(data.index);
        POP(assignStack);
        if (!r || !isNumeric(data.index->type)) {
          return false;
        }

        int arrType = data.left->type;
        ptr->type = typeTable[arrType].parent;
        return ptr->type != 0;
      }
    case AST_CALL:
      {
        struct AST_CALL data = ast.data.AST_CALL;
        bool r = traverse(data.identifier);
        if (!r) {
          return false;
        }
        // resolve data.identifier to string
        uint32_t leftType = data.identifier->type;
        TYPE_TABLE_ENTRY fnType = typeTable[leftType];
        if (fnType.parent != FN_INDEX) {
          compileError(data.identifier->token, "Attempting to call '");
          printTree(data.identifier);
          printf("' but it is not a function.\n");
          return false;
        }

        // Call should contain it's arguments
        if (arrlen(fnType.fields) < arrlen(data.arguments)) {
          int indent = compileError(data.identifier->token, "Too few arguments for function call of '");
          printTree(data.identifier);
          printf("'\n");
          printf("%*s", indent, "");
          printf("Expected %li argument(s) but instead found %li argument(s)\n", arrlen(data.arguments), arrlen(fnType.fields));
          return false;
        }
        if (arrlen(fnType.fields) > arrlen(data.arguments)) {
          int indent = compileError(data.identifier->token, "Too many arguments for function call of '");
          printTree(data.identifier);
          printf("'\n");
          printf("%*s", indent, "");
          printf("Expected %li argument(s) but instead found %li argument(s)\n", arrlen(data.arguments), arrlen(fnType.fields));
          return false;
        }

        for (int i = 0; i < arrlen(data.arguments); i++) {
          PUSH(typeStack, fnType.fields[i].typeIndex);
          PUSH(evaluateStack, true);
          r = traverse(data.arguments[i]);
          POP(evaluateStack);
          POP(typeStack);
          if (!r) {
            return false;
          }
          if (!isCompatible(fnType.fields[i].typeIndex, data.arguments[i]->type)) {
            int indent = compileError(data.arguments[i]->token, "Incompatible type for argument %i of '", i + 1);
            printTree(data.identifier);
            printf("'\n");
            printf("%*s", indent, "");
            printf("Expected type '%s' but instead found '%s'\n", typeTable[fnType.fields[i].typeIndex].name->chars, typeTable[data.arguments[i]->type].name->chars);
            return false;
          }
          data.arguments[i]->type = fnType.fields[i].typeIndex;
        }
        ptr->type = typeTable[leftType].returnType;
        return true;
      }
    default:
      {
        return true;
      }
  }
  return false;
}


bool resolveTree(AST* ptr) {

  SYMBOL_TABLE_init();
  PUSH(evaluateStack, true);
  PUSH(assignStack, false);
  bool success = resolveTopLevel(ptr);
  if (!success) {
    goto cleanup;
  }
  success &= traverse(ptr);
  if (!success) {
    goto cleanup;
  }
  SYMBOL_TABLE_calculateAllocations();

cleanup:
  if (options.report) {
    if (success) {
      printf("Resolution successful.\n");
    } else {
      printf("Resolution failed.\n");
    }
  }

  arrfree(evaluateStack);
  arrfree(assignStack);
  arrfree(typeStack);
  return success;
}

