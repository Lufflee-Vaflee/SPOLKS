#pragma once

#include <cstdint>
#include <limits>
#include "Utils.hpp"

namespace Protocol {

enum command_t : uint16_t {
    ECHO        = 0,
    TIME        = 1,
    UPLOAD      = 2,
    DOWNLOAD    = 3,
    CLOSE       = std::numeric_limits<uint16_t>::max()
};

constexpr uint16_t CUR_VERSION = 001;

struct Header {
    uint16_t version;
    command_t command;
    uint32_t payload_size;

    inline void endianTransform() {
        TransformEndianess(version, command, payload_size);
    }

    //payload
};

static_assert(sizeof(Header) == 8);

}

