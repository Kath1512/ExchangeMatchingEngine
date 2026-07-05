#pragma once
#include "networking/socket_utils.h"
#include <arpa/inet.h>
#include <iostream>

bool setup_server(int& socket_fd, int& client_fd);
bool setup_client(int& socket_fd);
