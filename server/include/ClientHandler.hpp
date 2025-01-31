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

        error_t error = m_ref.recieveMessage(m_socket, m_accumulate);
        if(error != 0 ) {
            return error;
        }

        while(m_accumulate.size() < sizeof(Header)) {
            auto head = deserialize<Header>(m_accumulate.begin()).first;
            if(head.version != CUR_VERSION) {
                data_t data;
                serialize(std::back_inserter(data), Header{
                    CUR_VERSION,
                    command_t::CLOSE,
                    0
                });

                m_ref.sendMessage(m_socket, data.begin(), data.end());
                return -1;
            }

            std::size_t expected_size = head.payload_size + sizeof(Header);

            if(m_accumulate.size() < expected_size) {
                return 0;
            }

            data_t query = std::move(m_accumulate);
            std::copy(query.begin() + expected_size, query.end(), m_accumulate.begin());
            query.resize(expected_size);
            m_service(query);
        }

        return 0;
    }

   private:
    ServerInterface& m_ref;
    socket_t m_socket;

    data_t m_accumulate;
    ClientService m_service;

   private:
    static constexpr std::size_t INITIAL_CAPACITY = 2048;
};
}
