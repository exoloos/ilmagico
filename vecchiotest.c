#include <stdio.h>
#include "smpcvecchia.h"
#include "bitut.h"

int main(int argc, char const *argv[]) {
  gate a;
  ClearAll(a.x0);
  ClearAll(a.x1);
  ClearAll(a.y0);
  ClearAll(a.y1);
  ClearAll(a.lut[0]);
  ClearAll(a.lut[1]);
  ClearAll(a.lut[2]);
  ClearAll(a.lut[3]);
  a.lut[0][0] = '0';
  a.lut[1][0] = '1';
  a.lut[2][0] = '1';
  a.lut[3][0] = '1';
  a.x0[16] = 'a';
  a.x1[16] = 'b';
  a.y0[16] = 'c';
  a.y1[16] = 'd';
  a.lut[0][16] = a.x0[16] ^ a.y0[16];
  a.lut[1][16] = a.x0[16] ^ a.y1[16];
  a.lut[2][16] = a.x1[16] ^ a.y0[16];
  a.lut[3][16] = a.x1[16] ^ a.y1[16];
  evgate input;
  kcopy(input.lut[0], a.lut[0]);
  kcopy(input.lut[1], a.lut[1]);
  kcopy(input.lut[2], a.lut[2]);
  kcopy(input.lut[3], a.lut[3]);

  kcopy(input.x, a.x1);
  kcopy(input.y, a.y1);
  char output[17];
  evaluate(input, output);
  printf("%c\n", output[0]);
  return 0;
}
