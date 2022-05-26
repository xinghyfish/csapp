#include <stdio.h>

int isAsciiDigit(int x) {
  int mask1 = ~0x3f;
  int mask2 = 0x3;
  int mask3 = 0x8;
  int mask4 = 0x6;
  return !(x & mask1) & !((x >> 4) ^ mask2) & (!(x & mask3) | !(x & mask4));
}

int conditional(int x, int y, int z) {
  int p = ~(!!x) + 1;
  return (p | z) & (~p | y);
}

int isLessOrEqual(int x, int y) {
  int p = x >> 31;
  int q = y >> 31;
  int r = (y - x) >> 31;
  return (!(x ^ y)) | (p & !q) | (!p & q) | (!r);
}

int howManyBits(int x) {
  int sign = x >> 31;
  x = (x & ~sign) | (sign & ~x);
  
  int bit16 = (!!(x >> 16)) << 4;
  x = x >> bit16;
  
  int bit8 = (!!(x >> 8)) << 3;
  x = x >> bit8;
  
  int bit4 = (!!(x >> 4)) << 2;
  x = x >> bit4;
  
  int bit2 = (!!(x >> 2)) << 1;
  x = x >> bit2;
  
  int bit1 = (!!(x >> 1));
  x = x >> bit1;

  int bit0 = x;

  return bit16 + bit8 + bit4 + bit2 + bit1 + bit0 + 1;
}

int floatFloat2Int(unsigned uf) {
  int sign = uf & 0x80000000;
  int exp = uf & 0x7f800000;
  int frac = (uf & 0x007fffff) | 0x00800000;
  int e = (exp >> 23) - 127;
  if (e > 30)
    return 0x80000000;
  if (e < 0) 
    return 0;
  int absint = 0;

  if (e < 23)
    absint = frac >> (23 - e);
  else
    absint = frac << (e - 23);
  
  return sign == 0x80000000 ? -absint : absint;
}

int main(int argc, char const *argv[])
{
    for (int i = 0; i < 100; ++i)
      printf("0x%x: %d\n", i, isAsciiDigit(i));
    printf("%d\n", isLessOrEqual(0xFFFFFFFE, 0xFFFFFFFF));
    printf("%d\n", howManyBits(-5));
    printf("%d\n", ((unsigned)-2)>>31);
    printf("%d\n", floatFloat2Int(0x425592a0));
    return 0;
}
