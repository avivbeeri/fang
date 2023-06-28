const message = 'Hello world' // Try edit me

let state = 0xAA;
let i = 0x01;
let j = 0x02;
let ns1 = [  0x01, 0x00 ];
let ns2 = [  0x01, 0x01 ];

function rrl(n) {
  const s = (n & 0xFF); 
  return (((s << 1) | (s >> 7)) & 0xFF);
}
function hash(state, i) {
  return rrl(state ^ i); 
}

// Log to console
for (n of ns1) {
  state = hash(state, n);
}
console.log(state);
state = 0xAA;
for (n of ns2) {
  state = hash(state, n);
}
console.log(state);
