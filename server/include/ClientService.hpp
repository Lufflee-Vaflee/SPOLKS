#pragma once

#include "Server.hpp"
#include "Protocol.hpp"

namespace tcp {

class ClientService {
    using const_iterator = data_t::const_iterator;

   public:
    data_t operator()(std::shared_ptr<data_t> bufferOwnership, const_iterator begin, const_iterator end) {
    }
};

}

