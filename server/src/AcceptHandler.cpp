#include "AcceptHandler.hpp"

#include "Protocol.hpp"

#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>

namespace tcp {

AcceptHandler::AcceptHandler(ServerInterface& ref, socket_t socket) :
    m_ref(ref),
    m_socket(socket) {}

error_t AcceptHandler::operator()(){
    std::lock_guard lock {m_socket};
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept4(m_socket, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);

    if (client_fd < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "EAGAIN EWOULDBLOCK happend after poll on accept socket\n";
            return 0;
        }

        perror("Poll accept request failed");
        return -1;
    }

    auto code = m_ref.registerConnection(client_fd);
    if(code < 0) {
        std::cout << "register connection failed socket descriptor already registered " << client_fd << '\n';
        return 0;
    }

    return 0;
}

}

