#pragma once

#include "config.hpp"

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

//no need in type-erasure and runtime interface
template <typename T>
concept Handler = 
    requires(T t) {
        { t.operator()() } -> std::same_as<error_t>;
    };
 
}
