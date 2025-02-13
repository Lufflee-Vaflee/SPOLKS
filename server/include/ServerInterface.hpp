#pragma once

#include "Server.hpp"

#include <unistd.h>

namespace tcp {

class ServerInterface {
   public:
    using const_iterator = std::vector<uint8_t>::const_iterator;

   public:
    virtual void closeConnection(socket_t socket) = 0;
    virtual error_t registerConnection(socket_t raw_socket) = 0;
    virtual error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end) = 0;
    virtual error_t recieveMessage(socket_t socket, data_t& buf) = 0;

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
