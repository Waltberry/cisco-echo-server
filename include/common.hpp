#pragma once

#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>

inline std::string errno_string() {
    return std::string(std::strerror(errno));
}

inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}
