# Fang - Programming Language

*WARNING*: Fang is not yet complete and should not be used for projects at this
time.

"First, and not good" - Aviv.

Fang is a programming language designed for abandoned embedded platforms
(primarily the Game Boy, NES, and other 8-bit architectures) It's weakly and
statically typed, imperative and procedural.

## Philosophy 
Fang is designed to be lightweight in feature-set, with minimal protections for 
the programmer. It's "sharp", so you need to approach it with discipline.

It assumes that you have total control over the hardware, and avoids certain
features which could cause issues if there were failures. That's why there is no 
dynamic memory management included. If it's needed, it could be implemented in a
platform library.

## Features:
  - Data types:
    - Signed and unsigned 8-bit values
    - Characters
    - Strings (literals only)
    - Pointers (platform-address wide)
    - Arrays
    - Records (Structs)
    - Unions (Tagged!)
  - Arithmetic Operations
  - Binary operations
  - Local and global variables
  - Control Flow: 
    - If-else statements
    - For loops (c-like)
    - while loops
    - do-while loops
    - match statements (for handling tagged unions)
  - Functions
  - Banked code/data blocks (and associated semantics)
  - Direct-to-output ASM block (for implementing platform libraries)
  - Module namespacing

## Platforms:

Fang currently only compiles to ARM64 (on Macbook M1), as a means of testing.
It's planned to run on:
  - Game Boy (z80-like)
  - NES (6502)

In the future, it might be good if it worked for:
  - AVR (Arduino)
  - ARM (for Pi Pico)
  - Motorola 68000-series (Palm Pilots)

## More Info:

I post about development of Fang on [my mastodon](https://mastodon.gamedev.place/@springogeek) 
as well as [my blog](https://infinitelimit.net)
