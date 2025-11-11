/*
 * Filename: d:\HeavenGate\src\common\Confparcer.h
 * Path: d:\HeavenGate\src\common
 * Created Date: Sunday, November 9th 2025, 10:53:02 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once

#include <unordered_map>
#include <string>
#include "Argparcer.h"
#include "logger.h"
#include "../../include/strconv.h"
#include <stdexcept>
#include "Confparcer.h"

#define HG_ENVKEY "HG_BASE"


enum ErrorCodes {
    SUCCESS = 0,
    CONFIG_NOT_OPENED = -4,
    NO_OPT_IN_CONFIG =-5
};

class Confparcer {
public:
    static Confparcer& the();
    
    int parce();
std::string get(const std::string& key, int* error_code) const;

template<typename T>
static T SETTING(const std::string& sett, const T& default_value = T{}) {
    std::string arg = Argparcer::Argparcer::the().get(sett);
    int e = 0;
    std::string conf = Confparcer::the().get(sett,&e);
    
    std::string value;
    
    // Приоритет: аргументы командной строки > конфиг файл > значение по умолчанию
    if (!arg.empty()) {
        value = arg;
    } else if (!conf.empty()) {
        value = conf;
    } else {
        logger::Logger::warn("Using default value for setting: " + sett);
        return default_value;
    }
    
    try {
        return convertFromString<T>(value);
    } catch (const std::invalid_argument& e) {
        logger::Logger::fatal("Conversion failed for setting '" + sett + "'. Value: '" + value + "', Expected type: " + typeid(T).name());
        return default_value;
    }
}
std::string getconfig() const;

private:
    Confparcer() = default;
    Confparcer(const Confparcer&) = delete;
    Confparcer& operator=(const Confparcer&) = delete;

    std::unordered_map<std::string, std::string> config;

};
