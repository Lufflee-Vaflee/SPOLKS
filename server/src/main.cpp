#include <iostream>
#include <csignal>

#include "ServerLinux.hpp"
#include "ThreadPool.hpp"

using namespace pool;

void signal_handler(int SIGNUM) {
    std::cout << "Shutting server down, SIGNUM: " << SIGNUM << "\n";
    DummyThreadPool::getInstance().stop();
}

int main() {
    using namespace std::chrono_literals;
    tcp::ServerConfig conf { 
        8080,
        50ms,
        5min,
        10min,
        10,
        128
    };

    tcp::Server<ENV> server{ conf };
    auto& pool = DummyThreadPool::getInstance();

    signal(SIGINT, signal_handler);

    pool.reserve_service([&server, &pool](atomic_state const& state){
        server.run(state);
        pool.stop();
    });

    pool.start();

    return 0;
}

