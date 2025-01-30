#pragma once

#include "ServerInterface.hpp"
#include "ClientService.hpp"
#include "Protocol.hpp"

#include <unistd.h>

#include <string>
#include <iostream>
#include <array>

namespace tcp {

class ClientHandler final : public ServerHandler {
   public:

    ClientHandler(ServerInterface& ref, socket_t socket) :
        ServerHandler(),
        m_ref(ref),
        m_socket(socket) {}

   private:
    virtual error_t operator()() override final {
        using namespace Protocol;

        error_t error = readAll();
        if(error != 0 ) {
            return error;
        }

        if(m_accumulate.size() < sizeof(Header)) {
            return 0;
        }

        auto head = deserialize<Header>(m_accumulate.begin()).first;
        if(head.version != CUR_VERSION) {
            data_t data;
            serialize(std::back_inserter(data), Header{
                CUR_VERSION,
                command_t::CLOSE,
                0
            });

            m_ref.sendMessage(m_socket, data);
            return 0;
        }

        std::size_t expected_size = head.payload_size + sizeof(Header);

        if(m_accumulate.size() < expected_size) {
            return 0;
        }

        return 0;
    }

   private:
    error_t readAll() {
        std::array<uint8_t, CAPACITY> buff;

        do {
            std::size_t bytes_read = read(m_socket, buff.data(), sizeof(buff) - 1);

            if(bytes_read == 0) {
                std::cout << "gracefull socket shutdown: " << m_socket << "\n";
                //possible ownership conflict after rewriting on multithreading, better remove close connection from interface and return close code
                m_ref.closeConnection(m_socket);        
                return -1;
            }

            if(bytes_read < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                }

                perror("Error while reading occured");
                return -1;
            }

            m_accumulate.insert(m_accumulate.end(), buff.begin(), buff.begin() + bytes_read);
        } while(true);

        return 0;
    }


   private:
    ServerInterface& m_ref;
    socket_t m_socket;

    data_t m_accumulate;

   private:
    static constexpr std::size_t CAPACITY = 2048;
};
}
