#pragma once

#include "ServerInterface.hpp"
#include "ClientService.hpp"
#include "Protocol.hpp"

#include <unistd.h>

namespace tcp {

class ClientHandler final : public ServerHandler {
   public:

    ClientHandler(ServerInterface& ref, socket_t socket) :
        ServerHandler(),
        m_ref(ref),
        m_socket(socket) {}

   public:
    virtual error_t operator()() override final {
        using namespace Protocol;

        error_t error = m_ref.recieveMessage(m_socket, m_accumulate);
        if(error != 0 ) {
            return error;
        }

        auto to_process = std::make_shared<data_t>(std::move(m_accumulate));
        std::size_t processed = 0;
        while(to_process->size() - processed < sizeof(Header)) {
            auto head = deserialize<Header>(to_process->begin()).first;
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

            uint32_t expected_size = head.payload_size + sizeof(Header);
            if(processed + expected_size > to_process->size()) {
                break;
            }

            auto begin = to_process->begin() + processed + sizeof(head);
            auto end = begin + expected_size;

            //will be threaded
            {
                data_t responce = service::processQuery(to_process, head.command, begin, end);
                m_ref.sendMessage(m_socket, responce.begin(), responce.end());
            }

            processed += expected_size;
        }

        if(processed == 0) {
            m_accumulate = std::move(*to_process);
            return 0;
        }

        //restore unprocessed tail
        if(processed < to_process->size()) {
            auto begin = to_process->begin() + processed;
            std::copy(begin, to_process->end(), m_accumulate.begin());
        }

        return 0;
    }

    void lock() {
        m_socket.lock();
    }

    void unlock() {
        m_socket.unlock();
    }

   private:
    ServerInterface& m_ref;
    SocketAccess m_socket;
    data_t m_accumulate;

   private:
    static constexpr std::size_t INITIAL_CAPACITY = 2048;
};
}
