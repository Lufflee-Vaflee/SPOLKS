#pragma once

#include "ServerInterface.hpp"
#include "ThreadPool.hpp"
#include "Protocol.hpp"

namespace tcp {

class ClientHandler final : public ServerHandler {
   public:
    ClientHandler(ServerInterface& ref, share_socket socket);

   public:
    virtual error_t operator()() override final;

   private:
    std::optional<Protocol::Header> process_head(data_t::const_iterator to_process);
    void produce_task(std::shared_ptr<data_t> to_process, data_t::const_iterator begin, data_t::const_iterator end, Protocol::command_t cmd);

   private:
    ServerInterface& m_ref;
    share_socket m_socket;
    data_t m_accumulate;
    pool::DummyThreadPool& m_pool = pool::DummyThreadPool::getInstance();

   private:
    static constexpr std::size_t INITIAL_CAPACITY = 2048;
};
}
