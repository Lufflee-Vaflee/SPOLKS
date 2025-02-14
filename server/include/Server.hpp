#pragma once

#include "config.hpp"

#if OS_ENV_CODE == 0 
#include "ServerLinux.hpp"
#elif OS_ENV_CODE == 1
#include "ServerWindows.hpp"
#else
static_assert(false);
#endif

