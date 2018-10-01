#include <stdio.h>

float fbar(float a, float b);
int main() {
  float foo = fbar(3, 4);
  printf("%f\n", foo);
  return 0;
}

