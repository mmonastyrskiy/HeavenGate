#include <string>
#pragma once

namespace utils{
static std::string MaskIPs(const std::string& data) {
    std::string result = data;
    size_t pos = 0;
    while ((pos = result.find("\"ip\": \"", pos)) != std::string::npos) {
        pos += 7;
        size_t dot_pos = result.find_last_of('.', result.find('"', pos));
        if (dot_pos != std::string::npos) {
            result.replace(dot_pos + 1, result.find('"', dot_pos) - dot_pos - 1, "xxx");
        }
    }
    return result;
}
};