PROJECT_ROOT="/Users/alex/Documents/test/c++/MyOrderBook"

cd "$PROJECT_ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target server client

# ./build/server &
# sleep 0.2
# ./build/client
