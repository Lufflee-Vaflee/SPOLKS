#pragma once

#include "Server.hpp"

#include "poll.h"
#include <string>

namespace tcp {

template<typename Server>
class HandlerInterface;

template<ENV_CONFIG CONFIG>
class HandlerInterface<Server<CONFIG>>{
    using Server_t = Server<CONFIG>;
    friend Server_t;

   private:
    HandlerInterface(Server_t& ref) :
        m_ref(ref) {}

   public:
    inline int closeConnection(int newClinetFD) const {
        m_ref.markAsClosed(newClinetFD);
    }

    inline int registerConnection(int newClientFD) const {
        m_ref.registerConnection(newClientFD);
    }

    inline int sendMessage(socket_t socket, std::string const& data) const {
        m_ref.sendMessage(socket, data);
    }

   private:
    Server_t& m_ref;
};

template<typename Server, typename ServerHandlerImpl>
class ServerHandler {
   public:
    inline int operator()(pollfd const& pollFD) {
        return static_cast<ServerHandlerImpl*>(this)->impl(pollFD);
    }

   private:
    virtual int impl(pollfd const& pollFD) {
        throw "Unimplemented exception";
    }
};


}
