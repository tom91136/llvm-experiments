CXX = clang++
CC = clang
CXXFLAGS = -fPIC -flto -shared -O3 -march=native -std=c++17 -Wall -Wextra -Wno-unused-parameter -g3
SAMPLE_CCFLAGS = -O3 
LLVM_FLAGS := $(shell llvm-config --cxxflags --ldflags --system-libs) # don't link any lib with `--libs all` as this a plugin

ifeq ($(OS), Windows_NT)
    $(error LLVM plugins are not supported on Windows)
endif

ifeq ($(shell uname), Darwin)
    CXXFLAGS += -Wl,-undefined,dynamic_lookup -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
    SAMPLE_CCFLAGS += -Wl,-undefined,suppress,-flat_namespace
    LIB_PTR_TRACER := libPtrTracer.dylib  
else
    LIB_PTR_TRACER := libPtrTracer.so  
endif

$(LIB_PTR_TRACER): plugin.cpp
	$(CXX) $(CXXFLAGS) plugin.cpp -o $(LIB_PTR_TRACER) $(LLVM_FLAGS)

test: foo.c bar.c $(LIB_PTR_TRACER)
	$(CC) foo.c bar.c -o test $(SAMPLE_CCFLAGS) -flto --ld-path=ld.lld -Wl,--load-pass-plugin,$(PWD)/$(LIB_PTR_TRACER)

.PHONY: clean
clean:
	rm -f $(LIB_PTR_TRACER) *.dSYM *.yaml test
