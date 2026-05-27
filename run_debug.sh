clang++ -std=c++20 \
-Iinclude \
-Itest \
-Wall -Wextra -Wpedantic \
-fsanitize=address,undefined \
-g \
src/*.cpp \
test/*.cpp \
test.cpp \
-o build/test && ./build/test