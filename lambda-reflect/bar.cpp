#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#include "lambda-reflect.h"

struct Foo {
  const char *x;
};

template <typename F> void demo(F f) {

  printf("demo>   %s %d %s %p\n",                     //
         lambda_reflect::get<const char *>(f, "b"), //
         lambda_reflect::get<int>(f, "a"),          //
         lambda_reflect::get<const char *>(f, "b"), //
         lambda_reflect::get<Foo>(f, "foo").x);

  f(1);

  lambda_reflect::set<int>(f, "a", 43);
  lambda_reflect::set<const char *>(f, "b", "bar");
  lambda_reflect::set<Foo>(f, "foo", Foo{nullptr});

  f(2);
}

int main() {

  int a = 42;
  const char *b = "foo";
  Foo foo = {b};

  printf("main>   %s %d %s %p\n", b, a, b, foo.x);

  demo([=](int arg) { printf("lam&> %d %s %d %s %p\n", arg, b, a, b, foo.x); });
  demo([&](int arg) { printf("lam=> %d %s %d %s %p\n", arg, b, a, b, foo.x); });

  printf("main>   %s %d %s %p\n", b, a, b, foo.x);
  printf("Done\n");
  return 0;
}