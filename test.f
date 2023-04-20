'H';
"hello world!\n";
asm {
  "mov X0, #1"
  "adr X1, const_1"
  "mov X2, #13"
  "mov X16, #4"
  "svc 0"
};
return 0 + 0 + 1;
