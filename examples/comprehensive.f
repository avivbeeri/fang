import "data"
// Not sure how modules will work yet. Concatenative source is
possible.


// Code will begin here.      
// Const must be evaluated at compile-time because it is held in ROM.
const count: uint8 = 5;

// "var" is a variable value, stored in RAM.
var num: uint8; // uninitialised value sets it to 0

num = fib(count);
var enabled: bool = true;
var num2: uint8 = v;
var text: str = "This is a string and it is null terminated.";

var list: uint8[8] = [1,2,3,4,5,6,7,8];

/*
  We are C-like so this comment style is perfect
*/


// ASM block outputs verbatim for assembly.
// This depends on the assembler used I think.
foreign asm {
  lda a, $45 ; 
};

type Entity: {
  id: uint8;
};

fn Entity::update(): void {
  // do something
}

var e: Entity;
e.id = 5;
e.update();


// two passes because we don't want to care about function ordering.
fn fib(n: uint8): uint16 {
  if (n == 0) {
    return 0;
  }
  if (n < 2) {
    return 1;
  }
    
  return fib(n-1) + fib(n - 2);
}

