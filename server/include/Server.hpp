#pragma once

#include "config.hpp"
#include <cstdint>
#include <chrono>

namespace tcp {

using socket_t = int;
using data_t = std::vector<uint8_t>;
using error_t = long;

struct ServerConfig {
    using duration = std::chrono::milliseconds;

    std::uint16_t port;
    duration pollTimeout;
    duration defaultSocketTimeout;
    duration forceClearDuration;
    std::uint16_t forceClearPercent;
    std::uint16_t backlog;
};

template<ENV::CONFIG_t>
class Server {
    static_assert(false);   //should use specialization
};

}

