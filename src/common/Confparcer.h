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

#define CONF_PATH "../../config/config.ini"


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


private:
    Confparcer() = default;
    Confparcer(const Confparcer&) = delete;
    Confparcer& operator=(const Confparcer&) = delete;

    std::unordered_map<std::string, std::string> config;
};
