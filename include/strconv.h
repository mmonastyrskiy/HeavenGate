#pragma once
#include <string>
#include <sstream>
#include <stdexcept>

namespace utils {

// Основной шаблон
template<typename T>
inline T convertFromString(const std::string& str) {
    T result;
    std::istringstream iss(str);
    
    if (!(iss >> result)) {
        throw std::invalid_argument("Cannot convert string to requested type");
    }
    
    // Проверяем, что вся строка была обработана
    char remaining;
    if (iss >> remaining) {
        throw std::invalid_argument("Extra characters in string conversion");
    }
    
    return result;
}

// Специализация для std::string - ДОБАВИТЬ inline
template<>
inline std::string convertFromString<std::string>(const std::string& str) {
    return str;
}

// Специализация для bool - ДОБАВИТЬ inline
template<>
inline bool convertFromString<bool>(const std::string& str) {
    if (str == "1" || str == "true" || str == "TRUE" || str == "on" || str == "ON") {
        return true;
    }
    if (str == "0" || str == "false" || str == "FALSE" || str == "off" || str == "OFF") {
        return false;
    }
    throw std::invalid_argument("Cannot convert to bool: " + str);
}

// TODO: УДАЛИТЬ специализацию для const char* - она опасна!
 template<>
 inline const char* convertFromString<const char*>(const std::string& str) {
     return str.c_str();  // ОПАСНО! Временный объект
 }

} // namespace utils