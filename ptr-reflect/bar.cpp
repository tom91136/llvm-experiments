#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "rt_reflect.hpp"

void show(void *p);

void dump(const std::string &name, void *p) {
  auto meta = ptr_reflect::reflect(p);
  printf("%-20s = %-16p size=%-8ld type=%-8s\n", name.c_str(), p, meta->size, to_string(meta->type));
}

int main(int argc, char **argv) {

  auto mallocPtr = malloc(sizeof(int) * 8);
  auto newPtr = new int;

  dump("mallocPtr", mallocPtr);
  dump("newPtr", newPtr);

  int stack = 42;
  dump("stackPtr", &stack);

  std::array<std::string, 128> strArray{};
  dump("stackArray.data()", strArray.data());
  dump("stackArray", &strArray);

  std::string str = "aaaaaa";
  std::string heapStr = str + str + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  dump("strPtr", &str);
  dump("heapStr", &heapStr);

  dump("strPtr.data() ", str.data());
  dump("heapStr.data()", heapStr.data());

  std::vector<std::string> vector;
  vector.emplace_back("foo");
  vector.emplace_back("bar");

  dump("vector", &vector);
  dump("vector.data()", vector.data());

  for (size_t i = 0; i < vector.size(); ++i) {
    dump("vector[" + std::to_string(i) + "]", &vector[i]);
    dump("vector[" + std::to_string(i) + "].data()", vector[i].data());
    dump("vector[" + std::to_string(i) + "].data()+2", vector[i].data() + 2);
  }

  show(&str);
  show(&stack);
  show(&mallocPtr);
  show(&newPtr);

  delete newPtr;
  free(mallocPtr);
  printf("Done\n");

  return EXIT_SUCCESS;
}