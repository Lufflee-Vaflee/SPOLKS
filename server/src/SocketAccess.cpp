#include "SocketAccess.hpp"

#include <unistd.h>

namespace tcp {

SocketAccess::SocketAccess(socket_t socket) :
    m_socket(socket) {}

void SocketAccess::lock() const {
    m_mutex.lock();
}

void SocketAccess::unlock() const {
    m_mutex.unlock();
}

SocketAccess::operator socket_t() const {
    return m_socket;
}

SocketAccess::~SocketAccess() {
    close(m_socket);
}

}


