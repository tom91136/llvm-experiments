# ptr-tracer

This is a LLVM pass plugin that traces the origin of pointers.
The plugin tries to locate the source of a pointer (e.g `alloca` or `malloc`) and reports on whether this can be conclusively determined.

To build the plugin only: 

```shell
make libPtrTracer.so
```
To run the plugin with test program:

```shell
make test 
```
A YAML report should be generated in the current directory.

## Usage

This plugin is intended to be used as a LTO LLD linker plugin:

```shell
# Full path to plugin required
clang hello.c foo.c -flto --ld-path=ld.lld -Wl,--load-pass-plugin,$PWD/libPtrTracer.so 
```

The report is saved as a YAML output in the same directory as the output.

For CMake projects, the plugin can be loaded with the following flags:

```
-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld;-Wl,--load-pass-plugin,$PWD/libPtrTracer.so"
-DCMAKE_MODULE_LINKER_FLAGS="-fuse-ld=lld;-Wl,--load-pass-plugin,$PWD/libPtrTracer.so"
-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld;-Wl,--load-pass-plugin,$PWD/libPtrTracer.so"
```


LLD on macOS is missing the `--load-pass-plugin` option **before** https://github.com/llvm/llvm-project/pull/115690; LLVM20 may include this change.
The ELF port of LLD has this implemented in https://revciews.llvm.org/D120490.
Like Clang, LLD on Windows does not seem to support plugins and will require further investigation.