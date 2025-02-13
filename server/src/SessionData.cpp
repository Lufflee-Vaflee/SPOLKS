#include "SessionData.hpp"

namespace tcp {

SessionData::SessionData(timePoint const& lastUse, share_socket socket, std::shared_ptr<ClientHandler> handler) :
    m_lastUse(lastUse.load()),
    m_socket(socket),
    m_clientHandler(handler),
    m_softClose(false) {}

SessionData::SessionData(SessionData&& other) :
    m_lastUse(other.m_lastUse.load()),
    m_socket(std::move(other.m_socket)),
    m_clientHandler(std::move(other.m_clientHandler)),
    m_softClose(other.m_softClose.load()) {}

SessionData& SessionData::operator=(SessionData&& other) {
    m_lastUse = other.m_lastUse.load();
    m_socket = std::move(other.m_socket);
    m_clientHandler = std::move(other.m_clientHandler);
    m_softClose = other.m_softClose.load();

    return *this;
}

}

