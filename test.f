/*
asm {
  "mov X0, #1"
  "adr X1, const_0 + 1"
  "adr X2, const_0"
  "ldrb W2,[X2, 0]"
  "mov X16, #4"
  "svc 0"
};
*/

fn test(a: int, b: int): int {}
var b: int = 5;
while (b) {
  b = b - 1;
}
for (var a: int = 0; a < 10; a = a + 1) {
  b = b + 1;
}
return b;
