#pragma once

#include "ThreadPool.hpp"
#include "ServerInterface.hpp"
#include "SocketAccess.hpp"

namespace tcp {

class AcceptHandler final : public ServerHandler {
   public:
    AcceptHandler(ServerInterface& ref, socket_t socket);

   private:
    virtual error_t operator()() override final;

   private:
    ServerInterface& m_ref;
    SocketAccess m_socket;
    pool::DummyThreadPool& pool = pool::DummyThreadPool::getInstance();
};

}

