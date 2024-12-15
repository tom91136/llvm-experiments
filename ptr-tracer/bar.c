#include <stdio.h>
#include <stdlib.h>

void swallow(int *) __attribute__((weak));

int foo(int a);

__attribute__((noinline)) void bar(int *xs, int n, int *ys)  {
  for (int i = 0; i < n; i++) {
    xs[i + 1] = n;
  }
  ys[0] = xs[0];
}

__attribute__((noinline)) void foo2(int *xs, int n, int *ys)  {
  int m = 2;
  bar(xs, n, &m);
  bar(xs, n, ys);
  swallow(&m);
}

int main(int argc, char **argv) {

  int x = 42;
  swallow(&x);

  int *xs = malloc(sizeof(int) * 10);
  foo2(xs, 5, xs);
  bar(xs, 6, &argc);
  return 0;
}