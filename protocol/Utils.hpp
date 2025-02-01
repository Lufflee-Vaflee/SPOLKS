#pragma once

#include <vector>
#include <memory>
#include <bit>
#include <utility>

namespace Protocol {

using data_t = std::vector<uint8_t>;

//as default endianess for protocol choosed little_endian
constexpr bool NEED_ENDIAN_TRANSFORM = std::endian::native != std::endian::little;

namespace {
    using back_insert = std::back_insert_iterator<data_t>;
    using const_iterator = data_t::const_iterator;
}

template<typename T>
concept totally_trivial = 
    std::is_standard_layout_v<T> && 
    std::is_trivially_copyable_v<T>;

template <typename T>
concept needTransform = 
    totally_trivial<T> &&
    requires(T t) {
        { t.endianTransform() } -> std::same_as<void>;
    };


template<typename T>
back_insert serialize(back_insert it, T const& obj) {
    if constexpr (needTransform<T> && NEED_ENDIAN_TRANSFORM) {
        T transforming = obj;
        transforming.endianTransform();
        auto raw = reinterpret_cast<const uint8_t* const>(std::addressof(transforming));
        return std::copy(raw, raw + sizeof(T), it);
    }

    auto raw = reinterpret_cast<const uint8_t* const>(std::addressof(obj));
    return std::copy(raw, raw + sizeof(T), it);
}

template<typename T>
requires std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>
std::pair<T, const_iterator> deserialize(const_iterator it) {
    T result{};

    auto raw = reinterpret_cast<uint8_t*>(std::addressof(result));
    auto next = std::copy(it, it + sizeof(T), raw);

    if constexpr (needTransform<T> && NEED_ENDIAN_TRANSFORM) {
        result.endianTransform();
    }

    return std::pair<T, std::vector<uint8_t>::const_iterator>{ result, next };
}

template<typename T>
requires std::is_integral_v<T> || std::is_enum_v<T> 
inline T byteswap(T val) {
    if constexpr (std::is_enum_v<T>) {
        auto underlying = std::to_underlying(val);
        return static_cast<T>(std::byteswap(underlying));
    } else {
        return std::byteswap(val);
    }
}

template<typename... args_t>
requires (std::disjunction_v<std::is_arithmetic<args_t>, std::is_enum<args_t>> && ...)
inline void TransformEndianess(args_t&... args) {
    (byteswap(args),...);
}

}

