asm {
  ".global _start"
  ".align 2"
  "_start: mov X0, #1"
  "adr X1, helloworld"
  "mov X2, #13"
  "mov X16, #4"
  "svc 0"
};
asm {
  "mov X0, #0"
  "mov X16, #1"
  "svc 0"
};
asm {
  "helloworld: .ascii \"Hello World!\n\""
};
/*
type Sprite {
  x: int8;
  y: int8;
  alive: bool;
}

fn aMethod(i: string, g: uint8): void {
  somethingElse(a, b, c);

  asm {
    "ld a, b"
    "words: dsdf"
    "words: dsdf"
  };
//  var a: Sprite = [ 0, 0, true ];
  var a: ptr = "words
  on
  new lines";
  @(a + 5) = Sprite;
  a.x = f;
  var y: uint8 = a.y;
  return;
}
*/
