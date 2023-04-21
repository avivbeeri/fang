"hello world!, this is really long, what do you think?\n";
asm {
  "mov X0, #1"
  "adr X1, const_0 + 1"
  "adr X2, const_0"
  "ldrb W2,[X2, 0]"
  "mov X16, #4"
  "svc 0"
};
return 0;
