#pragma once

#include "config.hpp"
#include <cstdint>

namespace tcp {

struct ServerConfig {
    std::uint16_t port;
    std::uint64_t timeout;
};

template<ENV_CONFIG>
class Server {};

}
