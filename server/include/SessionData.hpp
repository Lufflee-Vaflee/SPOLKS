#pragma once

#include <atomic>
#include <chrono>

#include "SocketAccess.hpp"
#include "ClientHandler.hpp"

namespace tcp {

struct SessionData {
   public:
    using timePoint = std::atomic<std::chrono::time_point<std::chrono::system_clock>>;

   public:
    SessionData(timePoint const& lastUse, share_socket socket, std::shared_ptr<ClientHandler> handler);

    SessionData(SessionData const&) = delete;
    SessionData& operator=(SessionData const&) = delete;

    SessionData(SessionData&& other);
    SessionData& operator=(SessionData&& other);

    ~SessionData() = default;

   public:
    timePoint m_lastUse;
    share_socket m_socket;
    std::shared_ptr<ClientHandler> m_clientHandler;
    std::atomic<bool> m_softClose = false;
};

}

