#include <iostream>
#include <csignal>

#include "Server.hpp"

std::sig_atomic_t g_shutting_down = 0;

void signal_handler(int SIGNUM) {
    std::cout << "Shutting server down, SIGNUM: " << SIGNUM << "\n";
    g_shutting_down = 1;
}

int main() {
    Server<OS, 8080> server{};

    signal(SIGINT, signal_handler);
    server.run(g_shutting_down);

    std::cout << "hello from server\n";

    return 0;
}

