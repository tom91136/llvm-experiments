#include <stdio.h>

#include "rt_reflect.hpp"

int foo(int a) { return a + 1; }

void show(void *p) { printf("show %p, size=%ld\n", p, ptr_reflect::reflect(p)->size); }