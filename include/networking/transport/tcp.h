#pragma once
#include "networking/socket_utils.h"
#include "networking/client_state.h"
#include<vector>
#include<fcntl.h>
#include <arpa/inet.h>
#include <iostream>
#include<sys/event.h>
#include<sys/types.h>
#include<sys/time.h>
#include <mutex>
#include "networking/msg_parser.h"

using ClientStateList = std::unordered_map<int, ClientState>;

bool setup_server(MessageSink& sink, ClientStateList& fd_to_state, std::mutex& state_mutex);
bool setup_client(int& socket_fd);

