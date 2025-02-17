#include "ServerLinux.hpp"

#include <iostream>
#include <algorithm>

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

namespace tcp {

Server::Server(ServerConfig const& config) :
    m_config(config) {
    std::cout << "starting server\n";
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < -1) {
        std::cout << "Creating socket failed: " << errno << '\n';
        throw "Socket create failed";
    }

    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK | SO_REUSEADDR);

    m_address.sin_family = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port = htons(m_config.port);

    if (bind(m_socket, (struct sockaddr*)&m_address, sizeof(m_address)) < 0) {
        std::cout << errno;
        close(m_socket);
        throw "Bind Failed";
    }

    m_acceptHandler = std::make_unique<AcceptHandler>(*static_cast<ServerInterface*>(this), m_socket);
}

Server::~Server() {
    std::cout << "Server shutting down\n";
    close(m_socket);
}

void Server::start(atomic_state const& shutting_down) {
    if (listen(m_socket, m_config.backlog) < 0) {
        std::cout << errno << '\n';
        throw "Listen failed";
    }

    std::cout << "Server listening on port " << m_config.port << std::endl;

    pollfd server_pollfd = {m_socket, POLLIN | POLLPRI | POLLERR | POLLHUP, 0};
    m_pollingFD.push_back(server_pollfd);

    bool force = false;
    while(shutting_down == state_t::Started) {
        m_pool.go([this](){
            checkForClear();
        });

        std::shared_lock lock{m_indexingMutex};
        int activity = poll(m_pollingFD.data(), m_pollingFD.size(), m_config.pollTimeout.count());

        if (activity < 0) {
            perror("Poll failed");
            break;
        }

        if (activity == 0) {
            // Timeout occurred, you can perform other tasks
            std::cout << "No activity, waiting..." << std::endl;
            checkForClear();
            continue;
        }


        for(std::size_t i = 0; i < m_pollingFD.size(); ++i) {
            socket_t sock = m_pollingFD[i].fd;
            auto revents = m_pollingFD[i].revents;
            if(revents & POLLIN) {
                if(i == 0) {
                    m_pool.go([this](){
                        error_t code = (*m_acceptHandler)();

                        if (code < 0) {
                            std::cout << "critical error on accept socket, stopping server\n";
                            m_pool.stop();
                        }
                    });
                    continue;
                }

                auto it = m_FDindexing.find(sock);
                if(it == m_FDindexing.end()) {
                    throw "poll event raised while indexing data doesnt exist";
                }

                auto handler = it->second.m_clientHandler;
                m_pool.go([handler](){
                    (*handler)();
                });
                continue;
            }

            if((revents & POLLPRI) || (revents & POLLERR) || (revents & POLLHUP)) {
                perror("Socket error during poll, closing");
                closeConnection(sock);
                continue;
            }

            if(revents & POLLNVAL) {
                perror("poll on closed socket");
                break;
            }
        }

    }

    std::cout << "stopping server\n";
    closeAll();
}

void Server::closeConnection(socket_t socket) {
    shutdown(socket, SHUT_RD); //no error check, multiply shutdown calls is possible and expected behaviour

    m_pool.go([this, socket](){
        std::shared_lock lock{m_indexingMutex};
        auto it = m_FDindexing.find(socket);
        if(it == m_FDindexing.end()) {
            return;
        }

        //this writing may seams strange under shared_lock, write is protected by atomic
        //shared lock here works more as "non-invalidating" data-structures operations
        bool expected = false;
        if(it->second.m_softClose.compare_exchange_strong(expected, true)) {
            m_uncleardPolls++;
        }
   });

    return;
}

error_t Server::registerConnection(socket_t socket_raw) {
    std::unique_lock lock{m_indexingMutex};

    m_pollingFD.push_back({
        socket_raw,
        POLLIN,
        0
    });

    share_socket socket = std::make_shared<SocketAccess>(socket_raw);
    auto it = m_FDindexing.emplace( 
        socket_raw, 
        SessionData {
            clock::now(),
            socket,
            std::make_shared<ClientHandler>(*static_cast<ServerInterface*>(this), socket),
        }
    );

    if(it.second) {
        return -1;
    }

    return 0;
}

void Server::updateUsage(socket_t clientFD) {
    std::shared_lock lock{m_indexingMutex};
    auto it = m_FDindexing.find(clientFD);

    if(it == m_FDindexing.end()) {
       return; 
    }

    it->second.m_lastUse = clock::now();
}

//anyone
error_t Server::sendMessage(socket_t socket, const_iterator begin, const_iterator end) {
    m_pool.go([this, socket](){
        updateUsage(socket);
    });

    std::size_t size = end - begin;
    auto code = send(socket, begin.base(), size, 0);
    if(code == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        perror("send message failed");
        return code;
    }

    return 0;
}

error_t Server::recieveMessage(socket_t socket, data_t& buf) {
    constexpr std::size_t INITIAL_CAPACITY = 2048;

    m_pool.go([this, socket]() {
        updateUsage(socket);
    });

    std::size_t effective_size = buf.size();
    std::size_t amortize_size = INITIAL_CAPACITY;
    do {
        if(effective_size > ULLONG_MAX - amortize_size) {
            amortize_size = ULLONG_MAX - effective_size;
        }

        buf.resize(effective_size + amortize_size);  //couldn't use capacity, because technically its UB
        int bytes_read = read(socket, buf.begin().base() + effective_size, amortize_size);

        if(bytes_read == 0) {
            std::cout << "gracefull socket shutdown: " << socket << "\n";
            return -1;
        }

        if(bytes_read < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
               return 0; 
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

void Server::performPollClear() {
    std::cout << "initiating poll cleanup\n";

    ServerConfig::duration timeout = m_config.defaultSocketTimeout;
    std::size_t softClear = 0;
    std::size_t hardClear = 0;

    if(m_pollingFD.size() <= 1) {
        return;
    }

    std::erase_if(m_pollingFD, [this, timeout, &softClear, &hardClear](pollfd poll) {
        if(poll.fd == m_socket) {
            return false;
        }

        auto it = m_FDindexing.find(poll.fd);
        if(it == m_FDindexing.end()) {
            throw "Indexing error occured";
        }

        if(it->second.m_softClose) {
            hardClear++;
            m_FDindexing.erase(it);
            return true;
        }

        if((clock::now() - it->second.m_lastUse.load()) > timeout) {
            softClear++;
            closeConnection(poll.fd);
        }

        return false;
    });

    std::cout << "Soft clear polls: " << softClear << "\n";
    std::cout << "Expected: " << m_uncleardPolls << "\n";
    std::cout << "Hard clear polls: " << hardClear << "\n";

    m_uncleardPolls = 0;
    m_lastClear = clock::now();
}

void Server::checkForClear() {
    std::cout << "Check for clear\n";
    bool timeCheck = (clock::now() - m_lastClear) > m_config.forceClearDuration;
    bool percentCheck = m_uncleardPolls > static_cast<std::size_t>((static_cast<double>(m_pollingFD.size()) / 100) * m_config.forceClearPercent);

    if(timeCheck || percentCheck) {
        std::unique_lock lock{m_indexingMutex};
        performPollClear();
    }
}

void Server::closeAll() {
    std::cout << "closing all connections\n";
    checkForClear();
    std::cout << "closing active connections\n";
    m_pollingFD.clear();
    m_FDindexing.clear();
}

}

