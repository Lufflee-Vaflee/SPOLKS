#include "ClientHandler.hpp"

#include "ClientService.hpp"
#include "Protocol.hpp"

#include <unistd.h>

namespace tcp {

ClientHandler::ClientHandler(ServerInterface& ref, socket_t socket) :
    ServerHandler(),
    m_ref(ref),
    m_socket(socket) {}


error_t ClientHandler::operator()() {
    using namespace Protocol;
    std::lock_guard lock {m_socket};

    error_t code = m_ref.recieveMessage(m_socket, m_accumulate);
    if(code < 0) {
        m_ref.closeConnection(m_socket);
        return code;
    }

    auto to_process = std::make_shared<data_t>(std::move(m_accumulate));
    std::size_t processed = 0;
    std::atomic<uint32_t> context = 0;
    while(to_process->size() - processed >= sizeof(Header)) {
        auto o_head = process_head(to_process->begin() + processed);
        if(!o_head.has_value()) {
            return -1;
        }

        auto head = *o_head;
        auto it = to_process->begin() + sizeof(Header);

        uint32_t expected_size = head.payload_size + sizeof(Header);
        if(processed + expected_size > to_process->size()) {
            break;
        }

        socket_t raw = m_socket;
        auto begin = it;
        auto end = begin + expected_size - sizeof(Header);
        m_pool.go([to_process, begin, end, cmd = head.command, &ref = m_ref, socket = raw](){
            data_t responce = service::processQuery(to_process, cmd, begin, end);
            //no need in synchronization here
            //responces may be send in different order(close request too) and this is expected behaivour of protocol
            ref.sendMessage(socket, responce.begin(), responce.end());
        });

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

std::optional<Protocol::Header> ClientHandler::process_head(data_t::const_iterator it) {
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

            m_ref.sendMessage(m_socket, data.begin(), data.end());
            m_ref.closeConnection(m_socket);
        });

        return {};
    }

    return head;
}

   void produce_task(std::atomic<uint32_t>& context, std::shared_ptr<data_t> to_process, Protocol::Header const& head);
}
