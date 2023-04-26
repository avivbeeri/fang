# Fang - Programming Language

"First, and not good" - Aviv.

Fang is a programming language designed for embedded binary environments.
It's weakly and statically typed, imperative and procedural.

## Data types:
Fang supports the following atomic types: void (function results only), ptr, bool, uint8, uint16, int8, int16, string, char.
Fang also allows for the creation of composite datatypes.
Arrays of types will be supported.

## Symbols
Language constructs: { } , . : ;
Grouping: ( )
Comments: // /* */
Assignment: =
Pointer ops: $ @ (unary)
Mathematical operators: + - * / %  (- is also unary)
Bitwise operators: ~ | & << >>
Boolean operators: (unary) ! && ||
Equality Operators: == != >= <= > <

Unimplemented but planned: 
// Arrays
// Data type method sugar?
cons ::

## Keywords
import, const, var, ext, asm, type, fn, void, 
return, false, true, while, for, if, else, this, as

## Grammar

program -> topDecl* EOF
topDecl -> 
  | typeDecl 
  | enumDecl 
  | fnDecl 
  | declaration

declaration -> 
  | varDecl 
  | constDecl 
  | asmDecl;
  | statement;

typeDecl -> "type" IDENTIFIER "{" fields? "}";
enumDecl -> "enum" IDENTIFIER "{" IDENTIFER ("=" expression) ("," IDENTIFIER ("=" expression)?)* "}";
fnDecl -> "fn" function;
constDecl -> "const" IDENTIFIER ":" type ("=" expression)? ";" ;
varDecl -> "var" IDENTIFIER ":" type ("=" expression)? ";" ;
asmDecl -> "asm" "{" (STRING ";")* "}" ";";

statement  -> block
            | ifStmt
            | forStmt
            | returnStmt
            | whileStmt
            | exprStmt;

exprStmt   -> expression ";";
forStmt    -> "for" "(" 
              (varDecl | exprStmt | ";")
              expression? ";"
              expression? ")" statement;

ifStmt     -> "if" "(" expression ")" statement ("else" statement)?;
whileStmt  -> "while" "(" expression ")" statement;
returnStmt -> "return" (expression)? ";";
block      -> "{" declaration* "}";

expression -> assignment ;
assignment -> ( call "." )? IDENTIFIER ("[" logic_or "]")* "=" assignment | logic_or;
logic_or   -> logic_and ("||" logic_and )* ;
logic_and  -> equality ("&&" equality )* ;
equality   -> comparison ( ("!=" | "==") comparison )* ;
comparison -> term ( ( ">" | ">=" | "<" | "<=" ) term )*;
term       -> factor ( ("-" | "+") factor)*;
factor     -> unary ( ("/" | "*" | "%" ) unary )*;
unary      -> ("!" | "-" | "&" | "$" ) unary | call;
call       -> subscript ( "(" arguments? ")" | "." IDENTIFIER )*;
subscript  -> casted ( "[" logic_or "]" )*; 
casted     -> reference ( "as" type )* ; 
reference  -> ("@" | "$")? primary ;
primary    -> literal | IDENTIFIER | "(" expression ")" | "[" list_display? "]";
literal    -> "true" | "false" | "this" | NUMBER | STRING;  

or_list -> logic_or ( "," logic_or )* ( "," )? ;

type       -> ( "^" ) ("[" NUMBER "]") type | typename;
typename   -> "void" | "bool" | "ptr" | "i8" | "ut8" 
            | "i16" | "u16" | "char" | "string" | IDENTIFIER; 
function   -> IDENTIFIER "(" parameters? ")" ":" type block ;
parameters -> IDENTIFIER ":" type (, IDENTIFIER ":" type)* ;
arguments  -> expression (, expression)* ;
fields     -> IDENTIFIER ":" type ";" (IDENTIFIER ":" type ";")* ;

# Lexical Grammar
Fang ignores whitespace, and supports oneline and multiline comments. Multiline comments are nestable.
comments -> ("//" <any char> "\n" ) | ("/*" (<any char> | comment>) "*/" );

NUMBER     -> DIGIT+ ( "." DIGIT+ )? ;
STRING     -> "\"" <any char, backslash is used for escaping the quote>* "\"" ;
IDENTIFIER -> ALPHA ( ALPHA | DIGIT )* ;
ALPHA      -> "a" ... "z" | "A" ... "Z" | "_" ;
DIGIT      -> "0" ... "9" ;
