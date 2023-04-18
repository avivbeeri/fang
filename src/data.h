#ifndef data_h
#define data_h

typedef struct {
  size_t length;
  char* chars;
} STRING;

STRING* allocateString(char* chars, int length);

#endif
