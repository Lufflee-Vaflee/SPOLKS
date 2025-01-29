#pragma once

#include "config.hpp"
#include <cstdint>
#include <chrono>

namespace tcp {

using socket_t = int;
using data_t = std::vector<uint8_t>;
using error_t = int;

struct ServerConfig {
    using duration = std::chrono::duration<std::uint64_t>;

    std::uint16_t port;
    duration pollTimeout;
    duration defaultSocketTimeout;
    duration forceClearDuration;
    std::uint16_t forceClearPercent;
    std::uint16_t backlog;
};

template<ENV_CONFIG>
class Server {
    static_assert(false);   //should use specialization
};
}
