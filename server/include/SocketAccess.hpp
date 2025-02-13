#pragma once

#include "Server.hpp"

#include <mutex>

namespace tcp {

class SocketAccess {
   public:
    SocketAccess(socket_t socket);
    ~SocketAccess();

   public:
    void lock() const;
    void unlock() const;
    operator socket_t() const;

   private:
    socket_t m_socket;
    mutable std::mutex m_mutex;
};

using share_socket = std::shared_ptr<SocketAccess>;

}

