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

template<>
size_t convertFromString<size_t>(const std::string& str) {
    if (str.empty()) {
        throw std::invalid_argument("Empty string cannot be converted to size_t");
    }
    
    // Убираем пробелы по краям
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) {
        throw std::invalid_argument("String contains only whitespace");
    }
    
    size_t end = str.find_last_not_of(" \t");
    std::string trimmed = str.substr(start, end - start + 1);
    
    // Проверяем на отрицательные числа
    if (trimmed[0] == '-') {
        throw std::invalid_argument("Negative value cannot be converted to size_t: " + trimmed);
    }
    
    // Проверяем, что строка состоит только из цифр (и возможного префикса)
    bool has_digits = false;
    for (size_t i = 0; i < trimmed.length(); ++i) {
        if (std::isdigit(trimmed[i])) {
            has_digits = true;
        } else if (i > 0) {
            // Разрешаем только цифры после первого символа
            throw std::invalid_argument("Invalid character in size_t conversion: " + trimmed);
        }
    }
    
    if (!has_digits) {
        throw std::invalid_argument("No digits found in string: " + trimmed);
    }
    
    std::istringstream iss(trimmed);
    size_t result;
    
    if (!(iss >> result)) {
        throw std::invalid_argument("Cannot convert string to size_t: " + trimmed);
    }
    
    // Проверяем переполнение (для очень больших чисел)
    std::string reconstructed = std::to_string(result);
    if (reconstructed != trimmed) {
        throw std::invalid_argument("Value out of range for size_t: " + trimmed);
    }
    
    return result;
}