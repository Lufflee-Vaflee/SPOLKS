#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>

#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>

#include "config.hpp"
#include "AcceptHandler.hpp"
#include "ClientHandler.hpp"

namespace tcp {

struct socketMetaData {
    using timePoint = std::chrono::time_point<std::chrono::system_clock>;
    using pollIndex = std::vector<pollfd>::iterator;

    timePoint lastUse;
    pollIndex index;
    bool softClose;
};

template<ENV_CONFIG CONFIG>
requires (CONFIG.OS == OS_t::LINUX)
class Server<CONFIG> {
   private:
    using clock = std::chrono::system_clock;
    using timepoint = std::chrono::time_point<clock>;

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

    }

    ~Server() {
        std::cout << "Server shutting down\n";
        close(m_socket);
        closeAll();
    }

    void run(std::sig_atomic_t& shutting_down) {
        if (listen(m_socket, m_config.backlog) < 0) {
            std::cout << errno << '\n';
            throw "Listen failed";
        }

        std::cout << "Server listening on port " << m_config.port << std::endl;

        pollfd server_pollfd = {m_socket, POLLIN, 0};
        m_pollingFD.push_back(server_pollfd);

        while(shutting_down != 1) {
            int activity = poll(m_pollingFD.data(), m_pollingFD.size(), m_config.port);

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

   protected:
    using Interface = HandlerInterface<Server>;
    friend Interface;
    //A "little overenginiered" way of splitting class functionality access between 
    //server delegates(handlers) and other parts of program.
    //First one by that has additional, but limited access provided by 3 methods declared below.
    //protected key word has no direct usage here, just to draw the attention to this side-effect on class state

    int closeConnection(socket_t clientFD) {
        if(close(clientFD) < 0) {
            perror("Error closing socket");
            return -1;
        }

        auto it = m_FDindexing.find(clientFD);
        if(it == m_FDindexing.end()) {
            throw("Indexing error occured");
        }

        it->second.softClose = true;

        m_uncleardPolls++;
        return 0;
    }

    int registerConnection(socket_t clientFD) {
        m_pollingFD.push_back({
            clientFD,
            POLLIN,
            0
        });

        auto it = m_FDindexing.emplace( 
            clientFD, 
            socketMetaData {
                clock::now(),
                --m_pollingFD.end()
            }
        );

        if(it.second) {
            throw "Indexing error try registered same socket";
        }

        return 0;
    }

    void updateUsage(socket_t clientFD) {
        auto it = m_FDindexing.find(clientFD);

        if(it == m_FDindexing.find(clientFD)) {
            throw "Indexing error occured";
        }

        it->second.lastUse = clock::now();
    }

    int sendMessage(socket_t socket, std::string const& data);

   private:

    void checkForClear(bool force) {
        std::cout << "Check for clear\n";
        bool timeCheck = (clock::now() - m_lastClear) > m_config.forceClearDuration;
        bool percentCheck = m_uncleardPolls > static_cast<std::size_t>((static_cast<double>(m_pollingFD.size()) / 100) * m_config.forceClearPercent);
        if(force || timeCheck || percentCheck) {
            performPollClear();
        }
    }

    void performPollClear() {
        std::cout << "initiating poll cleanup\n";
        if(m_pollingFD.size() <= 1) {
            return;
        }

        ServerConfig::duration timeout = m_config.defaultSocketTimeout;
        std::size_t softClear = 0;
        std::size_t hardClear = 0;

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

        std::cout << "Soft clear polls: " << softClear << "\n";
        std::cout << "Expected: " << m_uncleardPolls << "\n";
        std::cout << "Hard clear polls: " << hardClear << "\n";

        m_uncleardPolls = 0;
        m_lastClear = clock::now();
    }

    void closeAll() {
        std::cout << "closing all connections\n";
        checkForClear(true);
        for(auto it = m_pollingFD.begin(); it != m_pollingFD.end(); ++it) {
            close(it->fd);
        }
        m_pollingFD.clear();
        m_FDindexing.clear();
    }


   private:
    socket_t m_socket;
    sockaddr_in m_address{};

    std::vector<pollfd> m_pollingFD;
    std::unordered_map<socket_t, socketMetaData> m_FDindexing;

    ServerConfig m_config;
    timepoint m_lastClear = clock::now();
    std::size_t m_uncleardPolls = 0;

    AcceptHandler<Server> m_acceptHandler = AcceptHandler<Server>(Interface(*this));
    ClientHandler<Server> m_clientHandler = ClientHandler<Server>(Interface(*this));
};

}

