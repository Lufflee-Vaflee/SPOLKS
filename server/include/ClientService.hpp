#pragma once

#include "Protocol.hpp"

namespace service {

using namespace Protocol;

using const_iterator = data_t::const_iterator;
using back_insert = std::back_insert_iterator<data_t>;

data_t getTime();

data_t getEcho(const_iterator begin, const_iterator end);

data_t getClose();

std::pair<data_t, error_t> processQuery(std::shared_ptr<data_t> bufferOwnership, command_t command, const_iterator begin, const_iterator end);

}

