#pragma once

#include <iostream>

#include "Server.hpp"
#include "Protocol.hpp"

namespace tcp {

namespace service {

using namespace Protocol;

using const_iterator = data_t::const_iterator;
using back_insert = std::back_insert_iterator<data_t>;

data_t getTime() {
    data_t responce;
    const auto start = std::time(nullptr);

    auto it = back_inserter(responce);
    it = serialize(it, Header{
        CUR_VERSION,
        TIME,
        sizeof(start)
    });

    serialize(it, start);
    return responce;
}

data_t getEcho(const_iterator begin, const_iterator end) {
    data_t responce;
    auto it = back_inserter(responce);

    std::uint32_t size = end - begin; // safe, cause payload size is uint32_t
    it = serialize(it, Header{
        CUR_VERSION,
        ECHO,
        size
    });

    std::copy(begin, end, responce.end());

    return responce;
}

data_t getClose() {
    data_t responce;
    char message[] = "Closing connection by request";
    auto it = serialize(std::back_inserter(responce), Header{
        CUR_VERSION,
        CLOSE,
        sizeof(message)
    });

    serialize(it, message);
    return responce;
}


std::pair<data_t, error_t> processQuery(std::shared_ptr<data_t> bufferOwnership, command_t command, const_iterator begin, const_iterator end) {
    try {
        switch(command) {
        case TIME:
            return {getTime(), 0};
            break;
        case ECHO:
            return {getEcho(begin, end), 0};
            break;
        case CLOSE:
            return {getClose(), -1};
            break;
        case UPLOAD:
            break;
        case DOWNLOAD:
            break;
        }
    } catch(std::exception exc) {
        std::cout << exc.what();
    }

    return {{}, 0};
}

};

}

