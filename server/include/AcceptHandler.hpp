#pragma once

#include "ServerHandler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>

namespace tcp {

template<typename Server>
class AcceptHandler final : public ServerHandler<Server, AcceptHandler<Server>> {
   public:
    using Interface = HandlerInterface<Server>;
    using Base = ServerHandler<Server, AcceptHandler<Server>>;

    AcceptHandler(Interface const& interface) :
        Base(),
        m_interface(interface) {}

   private:
    inline int impl(pollfd const& serverFD) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept4(serverFD.fd, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);

        if (client_fd >= 0) {
            m_interface.registerConnection(client_fd);
        } else {
            perror("Poll accept request failed");
            return -1;
        }

        m_interface.sendMessage(client_fd, "Oh hello there");

        return 0;
    }

   private:
    Interface m_interface;
};

}

