#include <iostream>
#include <csignal>

#include "ServerLinux.hpp"

std::sig_atomic_t g_shutting_down = 0;

void signal_handler(int SIGNUM) {
    std::cout << "Shutting server down, SIGNUM: " << SIGNUM << "\n";
    g_shutting_down = 1;
}

int main() {
    using namespace std::chrono_literals;
    tcp::ServerConfig conf { 
        8080, 
        5s,
        5min,
        10min,
        10,
        128
    };

    tcp::Server<ENV> server{ conf };

    signal(SIGINT, signal_handler);
    server.run(g_shutting_down);

    return 0;
}

