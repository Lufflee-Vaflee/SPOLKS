#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <memory>

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

struct SessionData {
    using timePoint = std::chrono::time_point<std::chrono::system_clock>;
    using pollIndex = std::vector<pollfd>::iterator;

    timePoint lastUse;
    pollIndex index;
    bool softClose;

    std::shared_ptr<ClientHandler> clientHandler;
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
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK | SO_REUSEADDR);

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

        pollfd server_pollfd = {m_socket, POLLIN, 0};
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

            for(std::size_t i = 0; i < m_pollingFD.size(); ++i) {
                if(m_pollingFD[i].revents & POLLIN) {
                    
                }
            }

            checkForClear(false);
        }

        std::cout << "stopping server\n";
        closeAll();
    }

   private:
    virtual error_t closeConnection(SocketAccess const& socket) override final {
        {
            std::lock_guard lock(socket);
            if(close(socket) < 0) {
                perror("Error closing socket");
                return -1;
            }
        }

        {
            std::lock_guard lock{m_indexingMutex};
            auto it = m_FDindexing.find(socket);
            if(it == m_FDindexing.end()) {
                throw("Indexing error occured");
            }

            it->second.softClose = true;
        }

        m_uncleardPolls++;
        return 0;
    }

    virtual error_t registerConnection(socket_t socket_raw) override final {
        //no need in lock, only run/accept thread accessing
        m_pollingFD.push_back({
            socket_raw ,
            POLLIN,
            0
        });

        {
            std::scoped_lock lock{m_indexingMutex};
            auto it = m_FDindexing.emplace( 
                socket_raw, 
                SessionData {
                    clock::now(),
                    --m_pollingFD.end(),
                    false,
                    std::make_shared<ClientHandler>(
                        *static_cast<ServerInterface*>(this), 
                        socket_raw
                    )
                }
            );

            if(it.second) {
                return -1;
            }
        }

        return 0;
    }

    //anyone
    void updateUsage(socket_t clientFD) {
        std::lock_guard lock{m_indexingMutex};
        auto it = m_FDindexing.find(clientFD);

        if(it == m_FDindexing.find(clientFD)) {
           return; 
        }

        it->second.lastUse = clock::now();
    }

    //anyone
    virtual error_t sendMessage(SocketAccess const& socket, const_iterator begin, const_iterator end) override final {
        updateUsage(socket);

        auto base = begin.base();
        std::size_t size = end - begin;

        error_t code;
        {
            std::lock_guard lock {socket};
            code = send(socket, base, size, 0);
        }
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
    virtual error_t recieveMessage(SocketAccess const& socket, data_t& buf) override final {
        constexpr std::size_t INITIAL_CAPACITY = 2048;

        std::size_t effective_size = buf.size();
        std::size_t amortize_size = INITIAL_CAPACITY;
        std::lock_guard lock{socket};
        updateUsage(socket);
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
        if(m_pollingFD.size() <= 1) {
            return;
        }

        ServerConfig::duration timeout = m_config.defaultSocketTimeout;
        std::size_t softClear = 0;
        std::size_t hardClear = 0;

        {
            std::lock_guard lock{m_indexingMutex};

            std::remove_if(m_pollingFD.begin() + 1, m_pollingFD.end(), [this, timeout, &softClear, &hardClear](pollfd poll) {
                auto it = m_FDindexing.find(poll.fd);
                if(it == m_FDindexing.end()) {
                    throw "Indexing error occured";
                }

                if(it->second.softClose) {
                    softClear++;
                    m_FDindexing.erase(it);
                    return true;
                }

                if((clock::now() - it->second.lastUse) > timeout) {
                    hardClear++;
                    close(poll.fd);
                    m_FDindexing.erase(it);
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

    mutable std::mutex m_indexingMutex;
    std::vector<pollfd> m_pollingFD;
    std::unordered_map<socket_t, SessionData> m_FDindexing;

    ServerConfig m_config;
    timepoint m_lastClear = clock::now();
    std::size_t m_uncleardPolls = 0;

    std::unique_ptr<ServerHandler> m_acceptHandler;
};

}

