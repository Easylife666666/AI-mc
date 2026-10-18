#pragma once

#include <spdlog/spdlog.h>

namespace aimc {

inline void initLogging() {
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::info);
}

}  // namespace aimc
