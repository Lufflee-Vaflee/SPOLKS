#pragma once

#include "ServerInterface.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>

namespace tcp {

class AcceptHandler final : public ServerHandler {
   public:
    AcceptHandler(ServerInterface& ref, socket_t socket) :
        ServerHandler(),
        m_ref(ref),
        m_socket(socket) {}

   private:
    virtual error_t operator()() override final {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept4(m_socket, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);

        if (client_fd >= 0) {
            m_ref.registerConnection(client_fd);
        } else {
            perror("Poll accept request failed");
            return -1;
        }

        m_ref.sendMessage(client_fd, "Oh hello there");

        return 0;
    }

   private:
    ServerInterface& m_ref;
    socket_t m_socket;
};

}

