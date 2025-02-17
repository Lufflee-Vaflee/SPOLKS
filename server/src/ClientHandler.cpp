#include "ClientHandler.hpp"

#include "ClientService.hpp"
#include "Protocol.hpp"

#include <unistd.h>
#include <iostream>


namespace tcp {

ClientHandler::ClientHandler(ServerInterface& ref, share_socket socket) :
    m_ref(ref),
    m_socket(socket) {}


error_t ClientHandler::operator()() {
    using namespace Protocol;
    std::lock_guard lock {*m_socket};

    error_t code = m_ref.recieveMessage(*m_socket, m_accumulate);
    if(code < 0) {
        m_ref.closeConnection(*m_socket);
        return code;
    }

    auto to_process = std::make_shared<data_t>(std::move(m_accumulate));
    std::size_t processed = 0;
    std::size_t size = to_process->size();
    while(processed + sizeof(Header) <= to_process->size()) {
        auto o_head = process_head(to_process->begin() + processed);
        if(!o_head.has_value()) {
            return -1;
        }

        auto head = *o_head;
        std::size_t payload_start = processed + sizeof(Header);

        if(payload_start + head.payload_size > to_process->size()) {
            break;
        }

        auto begin = to_process->begin() + payload_start;
        auto end = begin + head.payload_size;
        produce_task(to_process, begin, end, head.command);

        processed = payload_start + head.payload_size;
    }

    if(processed == to_process->size()) {
        return 0;
    }

    std::copy(to_process->cbegin() + processed, to_process->cend(), m_accumulate.begin());

    return 0;
}

using const_iterator = data_t::const_iterator;

std::optional<Protocol::Header> ClientHandler::process_head(const_iterator it) {
    using namespace Protocol;
    auto [head, _] = deserialize<Header>(it);

    if(head.version != CUR_VERSION || head.command == command_t::CLOSE) {
        m_pool.go([&m_ref = m_ref, &m_socket = m_socket]() {
            data_t data;
            serialize(std::back_inserter(data), Header{
                CUR_VERSION,
                command_t::CLOSE,
                0
            });
            //std::lock_guard lock {*m_socket};
            m_ref.sendMessage(*m_socket, data.begin(), data.end());
            m_ref.closeConnection(*m_socket);
        });

        return {};
    }

    return head;
}

void ClientHandler::produce_task(std::shared_ptr<data_t> to_process, const_iterator begin, const_iterator end, Protocol::command_t cmd) {
    m_pool.go([to_process, begin, end, cmd, &ref = m_ref, socket = m_socket](){
        auto [responce, code] = service::processQuery(to_process, cmd, begin, end);
        //no need in synchronization here
        //responces may be executed and sended in different order(close request too) and this is expected behaivour of protocol
        if(responce.size() != 0) {
            //std::lock_guard lock {*socket};
            std::string log;
            std::copy(responce.begin(), responce.end(), std::back_inserter(log));
            std::cout << "sended responce: " << log << std::endl;
            auto send_code = ref.sendMessage(*socket, responce.begin(), responce.end());
            code = send_code < 0 ? send_code : code;
        }

        if(code < 0) {
            ref.closeConnection(*socket);
        }
    });
}

}

