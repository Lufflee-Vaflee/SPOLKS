#pragma once

#include "ServerHandler.hpp"

namespace tcp {

template<typename Server>
class ClientHandler final : public ServerHandler<Server, ClientHandler<Server>> {
   public:
    using Interface = HandlerInterface<Server>;
    using Base = ServerHandler<Server, ClientHandler<Server>>;

    ClientHandler(Interface interface) :
        Base(),
        m_interface(interface) {}

   private:
    int impl(pollfd const& clientPoll) {
        return 0;
    }

   private:
    Interface m_interface;
};

}

