#pragma once

#include "Server.hpp"

#include "poll.h"
#include <string>

namespace tcp {


class ServerInterface {
   public:
    virtual error_t closeConnection(socket_t socket) = 0;
    virtual error_t registerConnection(socket_t socket) = 0;
    virtual error_t sendMessage(socket_t socket, data_t const& data) = 0;

   public:
    virtual ~ServerInterface() = default;
};

class ServerHandler {
   public:
    virtual error_t operator()() = 0;

   public:
    virtual ~ServerHandler() = default;
};

}
