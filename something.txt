#include <iostream>
#include <cstring>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

constexpr int PORT = 8080;
constexpr int BACKLOG = 5;
constexpr int MAX_CLIENTS = 100;
constexpr int TIMEOUT = 1000; // Timeout for poll in milliseconds

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    std::vector<pollfd> fds;
    pollfd server_pollfd = {server_fd, POLLIN, 0};
    fds.push_back(server_pollfd);

    while (true) {
        int activity = poll(fds.data(), fds.size(), TIMEOUT);

        if (activity < 0) {
            perror("Poll failed");
            break;
        }

        if (activity == 0) {
            // Timeout occurred, you can perform other tasks
            std::cout << "No activity, waiting..." << std::endl;
            continue;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    // New incoming connection
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd >= 0) {
                        std::cout << "New client connected: " << client_fd << std::endl;
                        fds.push_back({client_fd, POLLIN, 0});
                    } else {
                        perror("Accept failed");
                    }
                } else {
                    // Handle data from an existing client
                    char buffer[1024] = {0};
                    int bytes_read = read(fds[i].fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        std::cout << "Client " << fds[i].fd << ": " << buffer;
                        const char* response = "Message received\n";
                        send(fds[i].fd, response, strlen(response), 0);
                    } else {
                        if (bytes_read == 0) {
                            std::cout << "Client " << fds[i].fd << " disconnected." << std::endl;
                        } else {
                            perror("Read failed");
                        }
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        --i;
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}

