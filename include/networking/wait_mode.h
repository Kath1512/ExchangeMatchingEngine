#pragma once
#include <cstdlib>
#include <cstring>

// WAIT_MODE=block -> consumer threads sleep on a semaphore between items.
// WAIT_MODE=spin (default) -> consumer threads busy-poll, lowest latency.
inline bool read_block_mode_from_env(){
    const char* mode = std::getenv("WAIT_MODE");
    return mode != nullptr && std::strcmp(mode, "block") == 0;
}
