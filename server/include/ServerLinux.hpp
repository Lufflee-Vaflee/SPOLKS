#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <memory>
#include <shared_mutex>

#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>

#include "config.hpp"
#include "Server.hpp"
#include "ServerInterface.hpp"
#include "AcceptHandler.hpp"
#include "ClientHandler.hpp"

#include "ThreadPool.hpp"

namespace tcp {

namespace {

class SessionData {
   public:
    using timePoint = std::atomic<std::chrono::time_point<std::chrono::system_clock>>;
    using pollIndex = std::vector<pollfd>::iterator;

   public:
    SessionData(timePoint const& lastUse, std::shared_ptr<ClientHandler> handler) :
        m_lastUse(lastUse.load()),
        m_clientHandler(handler),
        m_softClose(false) {}

    SessionData(SessionData const&) = delete;
    SessionData& operator=(SessionData const&) = delete;

    SessionData(SessionData&& other) :
        m_lastUse(other.m_lastUse.load()),
        m_clientHandler(std::move(other.m_clientHandler)),
        m_softClose(other.m_softClose.load()) {}

    SessionData& operator=(SessionData&& other) {
        m_lastUse = other.m_lastUse.load();
        m_clientHandler = std::move(other.m_clientHandler);
        m_softClose = other.m_softClose.load();

        return *this;
    }

   public:
    timePoint m_lastUse;
    std::shared_ptr<ClientHandler> m_clientHandler;

    std::atomic<bool> m_softClose = false;
};

}

template<ENV_CONFIG CONFIG>
requires (CONFIG.OS == OS_t::LINUX)
class Server<CONFIG> final : public ServerInterface {
   private:
    using clock = std::chrono::system_clock;
    using timepoint = std::chrono::time_point<clock>;
    using atomic_state = pool::atomic_state;
    using state_t = pool::state_t;

   public:
    Server(ServerConfig const& config) :
        m_config(config) {
        std::cout << "starting server\n";
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket == -1) {
            std::cout << "Creating socket failed: " << errno << '\n';
            throw "Socket create failed";
        }

        int flags = fcntl(m_socket, F_GETFL, 0);
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

        m_address.sin_family = AF_INET;
        m_address.sin_addr.s_addr = INADDR_ANY;
        m_address.sin_port = htons(m_config.port);

        if (bind(m_socket, (struct sockaddr*)&m_address, sizeof(m_address)) < 0) {
            std::cout << errno << '\n';
            close(m_socket);
            throw "Bind Failed";
        }

        m_acceptHandler = std::make_unique<AcceptHandler>(*static_cast<ServerInterface*>(this), m_socket);
    }

    ~Server() {
        std::cout << "Server shutting down\n";
        close(m_socket);
        closeAll();
    }

    void run(atomic_state const& shutting_down) {
        if (listen(m_socket, m_config.backlog) < 0) {
            std::cout << errno << '\n';
            throw "Listen failed";
        }

        std::cout << "Server listening on port " << m_config.port << std::endl;

        pollfd server_pollfd = {m_socket, POLLIN | POLLPRI | POLLERR | POLLHUP, 0};
        m_pollingFD.push_back(server_pollfd);

        while(shutting_down == state_t::Started) {
            int activity = poll(m_pollingFD.data(), m_pollingFD.size(), m_config.pollTimeout.count());

            if (activity < 0) {
                perror("Poll failed");
                break;
            }

            if (activity == 0) {
                // Timeout occurred, you can perform other tasks
                std::cout << "No activity, waiting..." << std::endl;
                checkForClear(true);
                continue;
            }

            {
                std::shared_lock read_lock{m_indexingMutex};
                for(std::size_t i = 0; i < m_pollingFD.size(); ++i) {
                    socket_t sock = m_pollingFD[i].fd;
                    auto revents = m_pollingFD[i].revents;
                    if(revents & POLLIN) {
                        if(i == 0) {
                            pool.go([this](){
                                (*m_acceptHandler)();
                            });
                            continue;
                        }

                        auto it = m_FDindexing.find(sock);
                        if(it == m_FDindexing.end()) {
                            throw "poll event raised while indexing data doesnt exist";
                        }

                        auto handler = it->second.m_clientHandler;
                        pool.go([handler](){
                            (*handler)();
                        });
                        continue;
                    }

                    if((revents & POLLPRI) || (revents & POLLERR) || (revents & POLLHUP)) {
                        pool.go([this, sock](){
                            closeConnection(sock);
                        });
                        continue;
                    }

                    if(revents & POLLNVAL) {
                        perror("poll on closed socket");
                        continue;
                    }
                }
            }

            checkForClear(false);
        }

        std::cout << "stopping server\n";
        closeAll();
    }

   private:
    virtual void closeConnection(socket_t socket) override final {
        shutdown(socket, SHUT_RDWR); //no error check, multiply shutdown calls is possible and expected behaviour

        pool.go([this, socket](){
            std::shared_lock lock{m_indexingMutex};
            auto it = m_FDindexing.find(socket);
            if(it == m_FDindexing.end()) {
                return;
            }

            //this writing may seams strange under shared_lock, but this flag is atomic
            //shared mutexes here works more as "non-invalidating" data-structures operations
            it->second.m_softClose = true;
            m_uncleardPolls++;
        });

        return;
    }

    virtual error_t registerConnection(socket_t socket_raw) override final {
        std::unique_lock lock{m_indexingMutex};

        m_pollingFD.push_back({
            socket_raw,
            POLLIN,
            0
        });

        auto it = m_FDindexing.emplace( 
            socket_raw, 
            SessionData {
                clock::now(),
                std::make_shared<ClientHandler>(*static_cast<ServerInterface*>(this), socket_raw)
            }
        );

        if(it.second) {
            return -1;
        }

        return 0;
    }

    //anyone
    void updateUsage(socket_t clientFD) {
        std::shared_lock lock{m_indexingMutex};
        auto it = m_FDindexing.find(clientFD);

        if(it == m_FDindexing.end()) {
           return; 
        }

        it->second.m_lastUse = clock::now();
    }

    //anyone
    virtual error_t sendMessage(socket_t socket, const_iterator begin, const_iterator end) override final {
        pool.go([this, socket](){
            updateUsage(socket);
        });

        auto base = begin.base();
        std::size_t size = end - begin;

        auto code = send(socket, base, size, 0);
        if(code == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cout << "[socket]: " << socket << "EAGAIN or EWOULDBLOCK after poll happend, skipped\n";
                return 0;
            }

            perror("send message failed");
            return code;
        }

        return 0;
    }

    //anyone
    virtual error_t recieveMessage(socket_t socket, data_t& buf) override final {
        constexpr std::size_t INITIAL_CAPACITY = 2048;

        pool.go([this, socket]() {
            updateUsage(socket);
        });

        std::size_t effective_size = buf.size();
        std::size_t amortize_size = INITIAL_CAPACITY;
        do {
            if(effective_size > ULLONG_MAX - amortize_size) {
                amortize_size = ULLONG_MAX - effective_size;
            }

            buf.resize(effective_size + amortize_size);  //couldn't use capacity, because technically its UB
            int bytes_read = read(socket, buf.end().base(), amortize_size);

            if(bytes_read == 0) {
                std::cout << "gracefull socket shutdown: " << socket << "\n";
                //possible ownership conflict after rewriting on multithreading, better remove close connection from interface and return close code
                //this->closeConnection(socket);
                return -1;
            }

            if(bytes_read < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                }

                perror("Error while reading occured");
                return -1;
            }

            effective_size += bytes_read;
            buf.resize(effective_size); //restore real size
            if(buf.size() == ULLONG_MAX) {
                break;
            }

            if(bytes_read == amortize_size)  {
                if(amortize_size >= ((ULLONG_MAX / 2) - 1)) {
                    amortize_size = ULLONG_MAX;
                } else {
                    amortize_size *= 2;
                }
            }
        } while(true);

        return 0;
    }

   private:
    //only run thread
    void checkForClear(bool force) {
        std::cout << "Check for clear\n";
        bool timeCheck = (clock::now() - m_lastClear) > m_config.forceClearDuration;
        bool percentCheck = m_uncleardPolls > static_cast<std::size_t>((static_cast<double>(m_pollingFD.size()) / 100) * m_config.forceClearPercent);

        if(force || timeCheck || percentCheck) {
            performPollClear();
        }
    }

    //only run thread
    void performPollClear() {
        std::cout << "initiating poll cleanup\n";

        ServerConfig::duration timeout = m_config.defaultSocketTimeout;
        std::size_t softClear = 0;
        std::size_t hardClear = 0;

        {
            std::unique_lock lock{m_indexingMutex};

            if(m_pollingFD.size() <= 1) {
                return;
            }

            std::remove_if(m_pollingFD.begin() + 1, m_pollingFD.end(), [this, timeout, &softClear, &hardClear](pollfd poll) {
                auto it = m_FDindexing.find(poll.fd);
                if(it == m_FDindexing.end()) {
                    throw "Indexing error occured";
                }

                if(it->second.m_softClose) {
                    close(poll.fd);
                    hardClear++;
                    m_FDindexing.erase(it);
                    return true;
                }

                if((clock::now() - it->second.m_lastUse.load()) > timeout) {
                    softClear++;
                    if(shutdown(poll.fd, SHUT_RDWR) < 0) {
                        perror("shutdown time-out failed");
                    }

                    it->second.m_softClose = true;
                    return true;
                }

                return false;
            });
        }

        std::cout << "Soft clear polls: " << softClear << "\n";
        std::cout << "Expected: " << m_uncleardPolls << "\n";
        std::cout << "Hard clear polls: " << hardClear << "\n";

        m_uncleardPolls = 0;
        m_lastClear = clock::now();
    }

    //only run thread
    void closeAll() {
        std::cout << "closing all connections\n";
        checkForClear(true);
        std::cout << "closing active connections\n";
        for(auto it = m_pollingFD.begin(); it != m_pollingFD.end(); ++it) {
            close(it->fd);
        }
        m_pollingFD.clear();
        m_FDindexing.clear();
    }


   private:
    socket_t m_socket;
    sockaddr_in m_address{};

    mutable std::shared_mutex m_indexingMutex;
    std::vector<pollfd> m_pollingFD;
    std::unordered_map<socket_t, SessionData> m_FDindexing;

    ServerConfig const m_config;
    timepoint m_lastClear = clock::now();
    std::size_t m_uncleardPolls = 0;

    std::unique_ptr<ServerHandler> m_acceptHandler;

    pool::DummyThreadPool& pool = pool::DummyThreadPool::getInstance();
};

}

