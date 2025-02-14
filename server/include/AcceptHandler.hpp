#pragma once

#include "ThreadPool.hpp"
#include "ServerInterface.hpp"
#include "SocketAccess.hpp"

namespace tcp {

class AcceptHandler {
   public:
    AcceptHandler(ServerInterface& ref, socket_t socket);

   public:
    virtual error_t operator()();

   private:
    ServerInterface& m_ref;
    SocketAccess m_socket;
    pool::DummyThreadPool& pool = pool::DummyThreadPool::getInstance();

};

static_assert(Handler<AcceptHandler>);

}

