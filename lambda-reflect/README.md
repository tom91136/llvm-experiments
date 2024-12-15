# lambda-reflect

This is a Clang frontend plugin that implements partial reflection for lambda members.
Concretely, the following operation is made possible:

```cpp
int a = 42;
auto f = [&]() { printf("%d\n", a); };
printf("%d\n", lambda_reflect::get<int>(f, "a")); // "42"
f(); // "42"
lambda_reflect::set<int>(f, "a", 43);
f(); // "43"
```
Both capture by value and reference are supported.

To build the plugin only: 

```shell
make libLambdaReflect.so # libLambdaReflect.dylib on macOS
```
To run the plugin with test program:

```shell
make test
```

## Usage

This plugin is intended to be used as a Clang plugin:

```shell
clang bar.cpp -O3 -march=native -Xclang -load -Xclang $PWD/libLambdaReflect.so -Xclang -add-plugin -Xclang lambda-reflect
```