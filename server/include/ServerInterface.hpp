#pragma once

#include "Server.hpp"

#include "poll.h"

#include <mutex>

namespace tcp {

class SocketAccess {
   public:
    SocketAccess(socket_t socket) :
        m_socket(socket) {}

   public:
    void lock() const {
        m_mutex.lock();
    }

    void unlock() const {
        m_mutex.unlock();
    }

    operator socket_t() const {
        return m_socket;
    }

   private:
    socket_t m_socket;
    mutable std::mutex m_mutex;
};

class ServerInterface {
   public:
    using const_iterator = std::vector<uint8_t>::const_iterator;

   public:
    virtual error_t closeConnection(SocketAccess const& socket) = 0;
    virtual error_t registerConnection(socket_t raw_socket) = 0;
    virtual error_t sendMessage(SocketAccess const& socket, const_iterator begin, const_iterator end) = 0;
    virtual error_t recieveMessage(SocketAccess const& socket, data_t& buf) = 0;

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
