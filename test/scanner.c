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

#include <criterion/criterion.h>
#include "../src/scanner.c"

Test(scanner, init) {
  const char* source = "";
  initScanner(source);
  cr_expect(scanner.start == source);
  cr_expect(scanner.current == source);
  cr_expect(scanner.pos == 0);
  cr_expect(scanner.line == 1);
}

Test(scanner, isAlpha) {
  for (unsigned char c = '\0'; c < 'A'; c++) {
    cr_expect(!isAlpha(c), "isAlpha should only accepts alphabetical characters, and underscore");
  }
  for (unsigned char c = 'A'; c <= 'Z'; c++) {
    cr_expect(isAlpha(c), "Uppercase characters should be alpha");
  }
  for (unsigned char c = '['; c < '_'; c++) {
    cr_expect(!isAlpha(c), "isAlpha should only accepts alphabetical characters, and underscore");
  }
  cr_expect(isAlpha('_'), "Underscores should count as alpha characters");
  cr_expect(!isAlpha('`'), "isAlpha should only accepts alphabetical characters, and underscore");
  for (unsigned char c = 'a'; c <= 'z'; c++) {
    cr_expect(isAlpha(c), "lowercase characters are alpha characters");
  }
  for (unsigned char c = '{'; c <= 0x7F; c++) {
    cr_expect(!isAlpha(c), "isAlpha should only accepts alphabetical characters, and underscore");
  }
}

Test(scanner, isDigit) {
  for (unsigned char c = '\0'; c < '0'; c++) {
    cr_expect(!isDigit(c), "isDigit should only accept numeric characters");
  }
  for (char c = '0'; c <= '9'; c++) {
    cr_expect(isDigit(c), "isDigit accepts numeral characters only");
  }
  for (unsigned char c = ':'; c < 0x7F; c++) {
    cr_expect(!isDigit(c), "isDigit should only accept numeric characters");
  }
}

Test(scanner, isAtEnd) {
  const char* source = "abcd";
  initScanner(source);
  cr_expect(!isAtEnd(), "End has not been reached");
  scanner.current += 4;
  cr_expect(isAtEnd(), "At the end of the stream, isAtEnd should be true.");
}

Test(scanner, advance) {
  const char* source = "abcd";
  initScanner(source);
  char c = advance();
  cr_expect(c == 'a', "advance() returns the current character");
  cr_expect(scanner.pos == 1, "advance() increases the current character position by 1");
  cr_expect(scanner.current == source + 1, "advance() increases the current character position by 1");
}

Test(scanner, peek) {
  const char* source = "abcd";
  initScanner(source);
  cr_expect(peek() == 'a', "peek() should return the current character without advancing");
  cr_expect(scanner.pos == 0, "peek() does not change the position");
  cr_expect(scanner.current == source, "peek() does not change the position");
}
Test(scanner, peekNext) {
  const char* source = "abcd";
  initScanner(source);
  cr_expect(peekNext() == 'b', "peekNext() should return the next character without advancing");
  cr_expect(scanner.pos == 0, "peekNext() does not change the position");
  cr_expect(scanner.current == source, "peekNext() does not change the position");
}
Test(scanner, match) {
  const char* source = "a";
  initScanner(source);
  cr_expect(!match('b'), "match() returns false if the current letter is not the given one.");
  cr_expect(scanner.pos == 0);
  cr_expect(scanner.current == source);

  cr_expect(match('a'), "match() return true and advances if the character given matches current.");
  cr_expect(scanner.pos == 1);
  cr_expect(scanner.current == source + 1);

  cr_expect(match('\0') == false, "at end of stream, should return false.");
}

Test(scanner, makeToken) {
  const char* source = "abcdef";
  initScanner(source);
  advance();
  scanner.start = scanner.current;
  advance();
  advance();

  Token token = makeToken(TOKEN_STRING);

  cr_expect(token.type == TOKEN_STRING);
  cr_expect(token.start == source + 1);
  cr_expect(token.line == 1);
  cr_expect(token.length == 2);
  cr_expect(token.pos == 3);
}

Test(scanner, errorToken) {
  const char* source = "abcdef";
  initScanner(source);
  advance();
  scanner.start = scanner.current;
  advance();
  advance();

  char* message = "an error occurred";
  Token token = errorToken(message);

  cr_expect(token.type == TOKEN_ERROR);
  cr_expect(token.start == message);
  cr_expect(token.length == 17);
  cr_expect(token.line == 1);
  cr_expect(token.pos == 3);
}

Test(scanner, skipWhitespace) {
  const char* source = " \r\ta\nb//test\n/*comment*/c";
  initScanner(source);

  cr_expect(scanner.line == 1);
  skipWhitespace();
  cr_expect(*(scanner.current) == 'a');
  scanner.current++;
  skipWhitespace();
  cr_expect(scanner.line == 2);
  cr_expect(*(scanner.current) == 'b');
  scanner.current++;
  skipWhitespace();
  cr_expect(scanner.line == 3);
  cr_expect(*(scanner.current) == 'c');
}
