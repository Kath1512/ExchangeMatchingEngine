# ════════════════════════════════════════════════════════════════════════════════
#  CMakeLists.txt — C++ Exchange Matching Engine
#
#  CMake is a build system *generator*. It doesn't compile your code directly;
#  it reads this file and produces the actual build files (e.g. Makefiles or
#  Ninja files) that do the compiling. You only need to re-run CMake when this
#  file changes. Normal code edits just need a `cmake --build build`.
# ════════════════════════════════════════════════════════════════════════════════

# The minimum CMake version required to process this file.
# 3.20 introduced better C++20 support and preset files.
cmake_minimum_required(VERSION 3.20)

# Declares the project. Sets the name, version, and which languages are used.
# LANGUAGES CXX tells CMake to only look for a C++ compiler (not C, Fortran, …).
project(ExchangeMatchingEngine VERSION 0.1.0 LANGUAGES CXX)


# ── C++ standard ──────────────────────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 20)           # Compile everything as C++20.
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Hard error if the compiler can't do C++20.
set(CMAKE_CXX_EXTENSIONS OFF)        # Disable GNU/MSVC extensions — stick to
                                     # standard C++ only (more portable).

# Tells CMake to write a compile_commands.json into the build folder.
# This file lists the exact compiler flags used for every source file.
# Tools like clangd (IDE autocomplete), clang-tidy, and clang-uml (diagrams)
# read this file to understand your project without you configuring them manually.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# ── Sanitizer helper ──────────────────────────────────────────────────────────
#
# An INTERFACE library holds no source files — it's just a bundle of flags.
# Any target that links against "sanitizers" automatically inherits those flags.
# This avoids repeating -fsanitize=... on every single target.
#
# We only enable sanitizers in Debug builds. In Release builds this block is
# skipped, so there's no runtime overhead in production code.
#
# AddressSanitizer (ASan)   — detects heap/stack buffer overflows, use-after-free.
# UndefinedBehaviourSanitizer (UBSan) — detects signed overflow, null deref, etc.
# -fno-omit-frame-pointer   — keeps stack frames so crash reports are readable.

add_library(sanitizers INTERFACE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -g          # include debug symbols (file names, line numbers in errors)
    )
    target_link_options(sanitizers INTERFACE
        -fsanitize=address,undefined
    )
endif()


# ── Core library ──────────────────────────────────────────────────────────────
#
# Instead of compiling src/*.cpp into both executables separately,
# we compile them once into a static library (.a file).
# Both main_app and test_runner then link against this one compiled artefact.
#
# STATIC = compiled into a .a file that gets bundled into the final executable.
# (The alternative is SHARED = .dylib/.so, loaded at runtime — not needed here.)

add_library(order_book_lib STATIC
    src/order.cpp
    src/price_level.cpp
    src/order_book.cpp
    src/trade.cpp
    src/helpers.cpp
    src/event_consumer.cpp
    src/order_event.cpp
    src/message_consumer.cpp
)

# target_include_directories tells the compiler where to find #include files.
#
# PUBLIC means this setting applies to:
#   1. order_book_lib itself when compiling.
#   2. Any other target that links against order_book_lib.
# So test_runner and main_app automatically get -Iinclude without declaring it.
#
# (The alternative keywords are:
#   PRIVATE  — only for this target, not propagated.
#   INTERFACE — only for targets that link against this, not for this target itself.)

target_include_directories(order_book_lib PUBLIC include)

# Warning flags. PRIVATE = only applied when compiling order_book_lib,
# not inherited by targets that link against it.
target_compile_options(order_book_lib PRIVATE
    -Wall       # common warnings
    -Wextra     # extra warnings
    -Wpedantic  # strict standard conformance warnings
)

# Link the sanitizer flags into the library (and transitively into anything
# that links order_book_lib, because sanitizers uses INTERFACE).
target_link_libraries(order_book_lib PUBLIC sanitizers)


# ── Main executable ───────────────────────────────────────────────────────────
#
# main.cpp is the scratchpad / manual integration testing entry point.
# It links order_book_lib, which automatically brings in -Iinclude too.

add_executable(main_app main.cpp)

target_compile_options(main_app PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(main_app PRIVATE order_book_lib)


# ── Test executable ───────────────────────────────────────────────────────────
#
# test.cpp is the test entry point. test/data_driven.cpp is the framework
# implementation — both need to be compiled into the same executable.

add_executable(test_runner
    test.cpp
    test/data_driven.cpp
)

# PRIVATE: the test/ include path is only needed by test_runner itself,
# not by anything that would link against test_runner (nothing does).
target_include_directories(test_runner PRIVATE test)

target_compile_options(test_runner PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(test_runner PRIVATE order_book_lib)


# ── Networking library ────────────────────────────────────────────────────────
#
# Compiles all networking code (sender, parser, transport) into a static lib.
# Depends on order_book_lib for Event, EventSink, etc.

add_library(networking_lib STATIC
    networking/event_sender.cpp
    networking/event_parser.cpp
    networking/msg_sender.cpp
    networking/msg_parser.cpp
    networking/client_state.cpp
    networking/input_handler.cpp
    networking/transport/tcp.cpp
)

target_compile_options(networking_lib PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(networking_lib PUBLIC order_book_lib)


# ── Multi-client kqueue server executable ─────────────────────────────────────

add_executable(server_kqueue server_code/multi_server_kqueue.cpp)
target_compile_options(server_kqueue PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(server_kqueue PRIVATE networking_lib)


# ── Client executable ─────────────────────────────────────────────────────────

add_executable(client client_code/client.cpp)
target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(client PRIVATE networking_lib)


# ── CTest integration ─────────────────────────────────────────────────────────
#
# CTest is CMake's built-in test runner. After enabling it:
#   `ctest --test-dir build`            runs all tests.
#   `ctest --test-dir build -R pattern` runs tests matching the pattern.
#   `ctest --test-dir build -V`         verbose output.
#
# Each add_test registers one executable as one named test.
# A non-zero exit code = test failure.

enable_testing()

# Order book — data-driven tests (17 named cases)
add_test(NAME order_book_tests COMMAND test_runner)

# Ring buffer — standalone, only needs order_book_lib (ring_buffer.h lives there)
add_executable(test_ring_buffer test/test_ring_buffer.cpp)
target_include_directories(test_ring_buffer PRIVATE test)
target_compile_options(test_ring_buffer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(test_ring_buffer PRIVATE order_book_lib)
add_test(NAME ring_buffer_tests COMMAND test_ring_buffer)

# Event pipeline — tests OrderBook + EventSink integration
add_executable(test_event_pipeline test/test_event_pipeline.cpp)
target_include_directories(test_event_pipeline PRIVATE test)
target_compile_options(test_event_pipeline PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(test_event_pipeline PRIVATE order_book_lib)
add_test(NAME event_pipeline_tests COMMAND test_event_pipeline)
