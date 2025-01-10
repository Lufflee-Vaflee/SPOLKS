#pragma once

#include <iostream>
#include <cstring>
#include <vector>
#include <memory>

#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>

#include "config.hpp"

namespace tcp {

template<OS_t OS, std::size_t PORT = 8080, std::size_t TIMEOUT = 1000>
class Server;

}

#include "ServerHandler.hpp"

namespace tcp {

template<OS_t OS, std::size_t PORT, std::size_t TIMEOUT>
class Server {
   public:
    Server() {
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
        m_address.sin_port = htons(PORT);

        if (bind(m_socket, (struct sockaddr*)&m_address, sizeof(m_address)) < 0) {
            std::cout << errno << '\n';
            close(m_socket);
            throw "Bind Failed";
        }

    }

    ~Server() {
        std::cout << "stoping server\n";
        close(m_socket);
    }

    void run(std::sig_atomic_t& shutting_down) {
        if (listen(m_socket, BACKLOG) < 0) {
            std::cout << errno << '\n';
            throw "Listen failed";
        }

        std::cout << "Server listening on port " << PORT << std::endl;

        pollfd server_pollfd = {m_socket, POLLIN, 0};
        m_pollingFD.push_back(server_pollfd);

        while(shutting_down != 1) {

            int activity = poll(m_pollingFD.data(), m_pollingFD.size(), TIMEOUT);

            if (activity < 0) {
                perror("Poll failed");
                break;
            }

            if (activity == 0) {
                // Timeout occurred, you can perform other tasks
                std::cout << "No activity, waiting..." << std::endl;
                continue;
            }

            for(std::size_t i = 0; i < m_pollingFD.size(); ++i) {
                if(m_pollingFD[i].revents & POLLIN) {
                    
                }
            }
        }

        m_pollingFD.clear();
    }

   protected:
    using Interface = HandlerInterface<Server>;
    friend Interface;
    //A "little overenginiered" way of splitting class functionality access between 
    //server delegates(handlers) and other parts of program.
    //First one by that has additional, but limited access provided by 3 methods declared below.
    //protected key word has no direct usage here, just to draw the attention to this side-effect on class state
    
    int closeConnection(pollfd const& pollFD);
    int registerConnection(pollfd const& pollFD);
    int sendMessage(std::vector<uint8_t> const& data);

   private:
    int m_socket;
    sockaddr_in m_address{};
    std::vector<pollfd> m_pollingFD;

    AcceptHandler<Server> m_acceptHandler = AcceptHandler<Server>(Interface(*this));
    ClientHandler<Server> m_clientHandler = ClientHandler<Server>(Interface(*this));

    static constexpr unsigned BACKLOG = 128;
};

}

