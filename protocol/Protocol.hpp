#pragma once

#include <cstdint>
#include <limits>
#include <vector>

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

    //payload
};

static_assert(sizeof(Header) == 8);

namespace {
    using back_insert = std::back_insert_iterator<std::vector<uint8_t>>;
}

template<typename T>
requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
back_insert serialize(back_insert it, T const& obj) {
    auto raw = reinterpret_cast<const uint8_t* const>(&obj);
    return std::copy(raw, raw + sizeof(T), it);
}

template<typename T>
requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
std::pair<T, std::vector<uint8_t>::const_iterator> deserialize(std::vector<uint8_t>::const_iterator it) {
    T result{};

    auto raw = reinterpret_cast<uint8_t*>(&result);
    auto next = std::copy(it, it + sizeof(T), raw);

    return std::pair<T, std::vector<uint8_t>::const_iterator>{ result, next };
}

using iterator = std::vector<uint8_t>::iterator;

template<typename T>
requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && std::is_trivially_constructible_v<T>
std::pair<T& , iterator> map(iterator it) {
    auto raw = reinterpret_cast<uint8_t*>(it.base());

    auto result = new (raw)T{};

    return std::pair<T, iterator>{ *result, it + sizeof(T)};
}

}

