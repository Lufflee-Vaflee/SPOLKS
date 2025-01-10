#pragma once

#include "Server.hpp"

#include "poll.h"
#include <vector>
#include <cstdint>

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
    inline int closeConnection(pollfd const& pollFD) const {
        m_ref.closeConnection(pollFD);
    }

    inline int registerConnection(pollfd const& pollFD) const {
        m_ref.registerConnection(pollFD);
    }

    inline int sendMessage(std::vector<uint8_t> const& data) const {
        m_ref.sendMessage(data);
    }

   private:
    Server_t& m_ref;
};

template<typename Server, typename ServerHandlerImpl>
class ServerHandler {
   public:
    using Interface = HandlerInterface<Server>;

    inline int operator()(pollfd const& pollFD) {
        return static_cast<ServerHandlerImpl*>(this)->impl(pollFD);
    }

   private:
    virtual int impl(pollfd const& pollFD) {
        throw "Unimplemented exception";
    }
};

template<typename Server>
class AcceptHandler final : public ServerHandler<Server, AcceptHandler<Server>> {
   public:
    using Interface = HandlerInterface<Server>;
    using Base = ServerHandler<Server, AcceptHandler<Server>>;

    AcceptHandler(Interface interface) :
        Base(),
        m_interface(interface) {}

   private:
    int impl(pollfd const& pollDF) {
        m_interface.closeConnection(pollDF);
        return 0;
    }

   private:
    Interface m_interface;
};

template<typename Server>
class ClientHandler final : public ServerHandler<Server, ClientHandler<Server>> {
   public:
    using Interface = HandlerInterface<Server>;
    using Base = ServerHandler<Server, ClientHandler<Server>>;

    ClientHandler(Interface interface) :
        Base(),
        m_interface(interface) {}

   private:
    int impl(pollfd const& pollDF) {
        return 0;
    }

   private:
    Interface m_interface;
};

}
