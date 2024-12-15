CXX = clang++
CC = clang
CXXFLAGS = -fPIC -shared -O3 -march=native -std=c++17 -Wall -Wextra -Wno-unused-parameter -g3
SAMPLE_CCFLAGS = -std=c++17 -O3 -g3 -Wall -Wextra -Wno-unused-parameter -fsanitize=address -fsanitize=undefined
LLVM_FLAGS := $(shell llvm-config --cxxflags --ldflags --system-libs) # don't link any lib with `--libs all` as this a plugin

ifeq ($(OS), Windows_NT)
    $(error LLVM plugins are not supported on Windows)
endif

ifeq ($(shell uname), Darwin)
    CXXFLAGS += -Wl,-undefined,dynamic_lookup -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
    SAMPLE_CCFLAGS += -Wl,-undefined,dynamic_lookup.,-flat_namespace
    LIB_LAMBDA_REFLECT=libLambdaReflect.dylib  
else
    LIB_LAMBDA_REFLECT=libLambdaReflect.so  
endif

$(LIB_LAMBDA_REFLECT): plugin.cpp
	$(CXX) $(CXXFLAGS) plugin.cpp -o $(LIB_LAMBDA_REFLECT) $(LLVM_FLAGS)

test: bar.cpp $(LIB_LAMBDA_REFLECT)
	$(CC) bar.cpp -o test $(SAMPLE_CCFLAGS) -Xclang -load -Xclang $(PWD)/$(LIB_LAMBDA_REFLECT) -Xclang -add-plugin -Xclang lambda-reflect
	./test

.PHONY: clean
clean:
	rm -f $(LIB_LAMBDA_REFLECT) *.dSYM test
