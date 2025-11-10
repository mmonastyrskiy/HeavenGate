/*
 * Filename: d:\HeavenGate\include\strconv.h
 * Path: d:\HeavenGate\include
 * Created Date: Monday, November 10th 2025, 12:45:26 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once
#include <string>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <algorithm>

template<typename T>
T convertFromString(const std::string& str) {
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

// Специализация для std::string (просто возвращаем копию)
template<>
std::string convertFromString<std::string>(const std::string& str) {
    return str;
}

// Специализация для bool (поддержка "true"/"false", "1"/"0")
template<>
bool convertFromString<bool>(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    
    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes" || lowerStr == "on") {
        return true;
    }
    if (lowerStr == "false" || lowerStr == "0" || lowerStr == "no" || lowerStr == "off") {
        return false;
    }
    
    throw std::invalid_argument("Cannot convert string to bool: " + str);
}

// Специализация для const char* (возвращаем c-string)
template<>
const char* convertFromString<const char*>(const std::string& str) {
    return str.c_str();
}