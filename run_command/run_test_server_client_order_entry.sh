PROJECT_ROOT="/Users/alex/Documents/test/c++/MyOrderBook"

cd "$PROJECT_ROOT"

clang++ -std=c++20 -Iinclude networking/*.cpp src/*.cpp server_code/server.cpp -o build/server
clang++ -std=c++20 -Iinclude networking/*.cpp src/*.cpp client_code/client.cpp -o build/client

# ./build/server &
# sleep 0.2
# ./build/client