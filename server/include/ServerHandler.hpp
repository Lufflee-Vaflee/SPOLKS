#pragma once

#include <iostream>
#include "Server.hpp"

namespace tcp {

template<typename Server>
class HandlerInterface;

template<OS_t OS, std::size_t PORT, std::size_t TIMEOUT>
class HandlerInterface<Server<OS, PORT, TIMEOUT>>{
    using Server_t = Server<OS, PORT, TIMEOUT>;
    friend Server_t;
    template<typename server, typename impl> friend class ServerHandler;

   private:
    HandlerInterface(Server_t& ref) :
        m_ref(ref) {}

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

    ServerHandler(Interface interface) :
        m_interface(interface) {}

    inline int operator()(pollfd const& pollFD) {
        return static_cast<ServerHandlerImpl*>(this)->impl(pollFD);
    }

   private:
    int impl(pollfd const& pollFD) {
        throw "Unimplemented exception";
    }

   private:
    Interface m_interface;
};

template<typename Server>
class AcceptHandler final : public ServerHandler<Server, AcceptHandler<Server>> {
   public:
    using Base = ServerHandler<Server, AcceptHandler<Server>>;

    AcceptHandler(Base::Interface interface) :
        Base(interface) {}

   private:
    int impl(pollfd const& pollDF) {
        return 0;
    }
};

template<typename Server>
class ClientHandler final : public ServerHandler<Server, ClientHandler<Server>> {
   public:
    using Base = ServerHandler<Server, ClientHandler<Server>>;

    ClientHandler(Base::Interface interface) :
        Base(interface) {}

   private:
    int impl(pollfd const& pollDF) {
        return 0;
    }
};

}

