#pragma once

#include "ServerHandler.hpp"
#include <unistd.h>

#include <string>
#include <iostream>

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
    inline int impl(pollfd const& clientPoll) {
        std::optional<std::string> result = readAll(clientPoll.fd);

        if(!result.has_value()) {
            return -1;
        }
    }

   private:
    std::optional<std::string> readAll(int socket) {
        constexpr std::size_t CAPACITY = 1024;
        char buff[CAPACITY];

        
        std::string result;
        do {
            int bytes_read = read(socket, buff, sizeof(buff) - 1);
            if(bytes_read == 0) {
                std::cout << "gracefull socket shutdown: " << socket << "\n";
                m_interface.closeConnection(socket);
                return std::nullopt;
            }

            if(bytes_read < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                }

                perror("Error while reading occured");
                return std::nullopt;
            }

            buff[bytes_read] = '\0';
            result.append(buff);
        } while(true);

        return result;
    }


   private:
    Interface m_interface;
};

}

