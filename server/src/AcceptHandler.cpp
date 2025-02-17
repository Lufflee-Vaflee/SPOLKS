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

/*    using namespace Protocol;
    data_t responce;
    char message[] = "Oh hello there!";
    auto it = serialize(std::back_inserter(responce), Header{
        CUR_VERSION,
        ECHO,
        sizeof(message)
    });

    serialize(it, message);

    error_t code = m_ref.sendMessage(client_fd, responce.begin(), responce.end());
    if(code < 0) {
        std::cout << "initial send message failed closing connection immidieatly\n";
        close(client_fd);
    }
*/
    auto code = m_ref.registerConnection(client_fd);
    if(code < 0) {
        std::cout << "register connection failed socket descriptor already registered " << client_fd << '\n';
        return 0;
    }

    return 0;
}

}

