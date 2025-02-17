#pragma once

#include "config.hpp"
#include "ThreadPool.hpp"

#include <vector>
#include <unordered_map>
#include <chrono>
#include <shared_mutex>
#include <memory>

#include "ServerInterface.hpp"
#include "AcceptHandler.hpp"
#include "SessionData.hpp"

#include <poll.h>
#include <netinet/in.h>

namespace tcp {

class Server final : public ServerInterface {
   private:
    using clock = std::chrono::system_clock;
    using timepoint = std::chrono::time_point<clock>;
    using atomic_state = pool::atomic_state;
    using state_t = pool::state_t;

   public:
    Server(ServerConfig const& config);
    Server(Server const& other) = delete;
    Server& operator=(Server const& other) = delete;
    Server& operator=(Server&& other) = delete;
    ~Server();

   public:
    void start(atomic_state const& shutting_down);

   private:
    virtual void closeConnection(socket_t socket) override final;
    virtual error_t registerConnection(socket_t socket_raw) override final;
    virtual error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end) override final;
    virtual error_t recieveMessage(socket_t socket, data_t& buf) override final;

   private:
    void updateUsage(socket_t clientFD);
    void checkForClear();
    void performPollClear();
    void closeAll();

   private:
    socket_t m_socket;
    sockaddr_in m_address{};

    mutable std::shared_mutex m_indexingMutex;
    std::vector<pollfd> m_pollingFD;
    std::unordered_map<socket_t, SessionData> m_FDindexing;

    ServerConfig const m_config;
    timepoint m_lastClear = clock::now();
    std::size_t m_uncleardPolls = 0;

    std::unique_ptr<AcceptHandler> m_acceptHandler;

    pool::DummyThreadPool& m_pool = pool::DummyThreadPool::getInstance();

    static constexpr auto ENV_CONF = ENV::CONFIG;
};

}

