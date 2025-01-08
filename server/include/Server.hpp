#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>

#include "config.hpp"

template<OS_t OS, std::size_t PORT = 8080>
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

    void run(std::sig_atomic_t& shutting_down) {
        if (listen(m_socket, BACKLOG) < 0) {
            std::cout << errno << '\n';
            throw "Listen failed";
        }

        std::cout << "Server listening on port " << PORT << std::endl;

        while(shutting_down != 1) {
            sleep(1);
            socklen_t addrlen = sizeof(m_address);
            int client_socket = accept(m_socket, (struct sockaddr*)&m_address, &addrlen);
            if (client_socket < 0) {
                std::cout << "error accepting client: " << errno << " skipped\n";
                continue;
            }

            std::cout << "new client connected\n";
        }
    }

    ~Server() {
        std::cout << "stoping server\n";
        close(m_socket);
    }

   private:
    int m_socket;
    sockaddr_in m_address{};

    static constexpr unsigned BACKLOG = 128;
};
