CXX = clang++
CXXFLAGS = -fPIC -flto -shared -O3 -march=native -std=c++17 -Wall -Wextra -Wno-unused-parameter -g3
SAMPLE_CCFLAGS = -O3 -g
LLVM_FLAGS := $(shell llvm-config --cxxflags --ldflags --system-libs) # don't link any lib with `--libs all` as this a plugin


ifeq ($(OS), Windows_NT)
    $(error LLVM plugins are not supported on Windows)
endif

ifeq ($(shell uname), Darwin)
    CXXFLAGS += -Wl,-undefined,dynamic_lookup -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
    SAMPLE_CCFLAGS += -Wl,-undefined,dynamic_lookup,-flat_namespace
    LIB_PTR_REFLECT := libPtrReflect.dylib  
     
else
    LIB_PTR_REFLECT := libPtrReflect.so  
endif


$(LIB_PTR_REFLECT): plugin.cpp
	$(CXX) $(CXXFLAGS) $^ -o $(LIB_PTR_REFLECT) $(LLVM_FLAGS)

all: $(LIB_PTR_REFLECT)

test: foo.cpp bar.cpp $(LIB_PTR_REFLECT) $(LIB_PTR_REFLECT_RT)
	$(CXX) $(SAMPLE_CCFLAGS) -include rt.hpp foo.cpp bar.cpp -o test -fpass-plugin=$(PWD)/$(LIB_PTR_REFLECT)


.PHONY: clean
clean:
	rm -rf $(LIB_PTR_REFLECT) $(LIB_PTR_REFLECT_RT) *.dSYM *.yaml trace_*.json test

