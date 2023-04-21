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

#ifndef value_h
#define value_h

typedef enum {
  VAL_BOOL,
  VAL_CHAR,
  VAL_U8,
  VAL_I8,
  VAL_U16,
  VAL_I16,
  VAL_PTR,
  VAL_INT,
  VAL_STRING,
  VAL_ERROR,
} ValueType;

typedef struct Value {
  ValueType type;
  union {
    bool boolean;
    int64_t number;
    uint8_t u8;
    uint16_t u16;
    int8_t i8;
    int16_t i16;
    STRING* string;
    unsigned char character;
    uint16_t ptr;
  } as;
} Value;

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER(value) ((Value){VAL_INT, {.number = value }})
#define U8(value) ((Value){VAL_U8, {.u8 = value}})
#define I8(value) ((Value){VAL_I8, {.i8 = value}})
#define U16(value) ((Value){VAL_U16, {.u16 = value}})
#define I16(value) ((Value){VAL_I16, {.i16 = value}})
#define STRING(value) ((Value){VAL_STRING, {.string = value}})
#define CHAR(value) ((Value){VAL_CHAR, {.character = value}})
#define PTR(value) ((Value){VAL_PTR, {.ptr = value}})
#define ERROR(value) ((Value){VAL_ERROR, {.ptr = value}})

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_ERROR(value)    ((value).as.ptr)
#define AS_U8(value)  ((value).as.u8)
#define AS_I8(value)  ((value).as.i8)
#define AS_U16(value)  ((value).as.u16)
#define AS_I16(value)  ((value).as.i16)
#define AS_STRING(value)  ((value).as.string)
#define AS_CHAR(value)  ((value).as.character)
#define AS_PTR(value)  ((value).as.ptr)


#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_INT)
#define IS_U8(value)    ((value).type == VAL_U8)
#define IS_I8(value)    ((value).type == VAL_I8)
#define IS_U16(value)    ((value).type == VAL_U16)
#define IS_I16(value)    ((value).type == VAL_I16)
#define IS_STRING(value)    ((value).type == VAL_STRING)
#define IS_CHAR(value)    ((value).type == VAL_CHAR)
#define IS_PTR(value)    ((value).type == VAL_PTR)
#define IS_ERROR(value)    ((value).type == VAL_ERROR)

#define IS_NUMERICAL(value) ((value).type <= VAL_INT)

#endif
