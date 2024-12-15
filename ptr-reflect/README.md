# ptr-reflect

This is a LLVM pass plugin that adds reflection capability for all runtime pointers.
The plugin introduces a thin runtime that interposes memory management calls and also instruments all stack allocation and deallocations using lifetime markers. 
Concretely, the following operation is made possible:


```cpp

uint32_t stack;
auto heap = new uint32_t;
// ...
printf("%p=%ld\n", &stack, ptr_reflect::reflect(&stack)->size);
printf("%p=%ld\n", heap, ptr_reflect::reflect(heap)->size);
```

To build the plugin only: 

```shell
make libPtrReflect.so # or libPtrReflect.dylib on macOS
```
To run the plugin with test program:

```shell
make test && ./test
```
A trace_*.json file should be generated in the current directory.
This trace file can be viewed using <https://ui.perfetto.dev>

## Usage

This plugin is intended to be used as LLVM plugin:

```shell
# Full path to plugin required
clang hello.c foo.c -fpass-plugin=$PWD/libPtrReflect.so -include rt.hpp
```
You must include the runtime `rt.hpp` for reflection to work.
