#pragma once

#include "ServerInterface.hpp"
#include <unistd.h>

#include <string>
#include <iostream>

namespace tcp {

class ClientHandler final : public ServerHandler {
   public:

    ClientHandler(ServerInterface& ref, socket_t socket) :
        ServerHandler(),
        m_ref(ref),
        m_socket(socket) {}

   private:
    virtual error_t operator()() override final {
        auto result = readAll();

        if(!result.has_value()) {
            return -1;
        }
    }

   private:
    std::optional<data_t> readAll() {
        char buff[CAPACITY];

        
        data_t result;
        do {
            //int bytes_read = read(m_socket, buff, sizeof(buff) - 1);
            //if(bytes_read == 0) {
            //    std::cout << "gracefull socket shutdown: " << m_socket << "\n";
            //    m_ref.closeConnection(m_socket);
            //    return std::nullopt;
            //}

            //if(bytes_read < 0) {
            //    if(errno == EWOULDBLOCK || errno == EAGAIN) {
            //        break;
            //    }

            //    perror("Error while reading occured");
            //    return std::nullopt;
            //}

            //buff[bytes_read] = '\0';
            //result.append(buff);
        } while(true);

        return result;
    }


   private:
    ServerInterface& m_ref;
    socket_t m_socket;

    data_t m_accumulate;

   private:
    static constexpr std::size_t CAPACITY = 1024;
};
}
