/*
 * Filename: d:\HeavenGate\src\common\Confparcer.cpp
 * Path: d:\HeavenGate\src\common
 * Created Date: Sunday, November 9th 2025, 10:52:43 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#include "Confparcer.h"
#include <fstream>
#include "logger.h"
#include "generic.h"
#include <algorithm> // for std::remove
#include <cctype>    // for std::isspace
#include <cstdlib>
#include <filesystem>

Confparcer& Confparcer::the() {
    static Confparcer c;
    c.parce();
    LOG_INFO("Confparcer parce called");
    return c;
}

int Confparcer::parce() {
    std::string path = getconfig();
    LOG_DEBUG("Got config file " + path);
    std::ifstream cfile(path); 
    
    if (!cfile.is_open()) {
        LOG_FATAL("Failed to open config file, please check the path");
        return ErrorCodes::CONFIG_NOT_OPENED;
    }
    LOG_DEBUG("Opened config file " + path);
    
    std::string line;
    int line_num = 1;
    
    while (std::getline(cfile, line)) {
        // Trim whitespace from both ends
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            line_num++;
            continue;
        }
        
        // Find equals sign
        size_t div = line.find('=');
        if (div == std::string::npos) {
            LOG_WARN("Illegal line in config at line " + std::to_string(line_num));
            line_num++;
            continue;
        }
        
        // Extract key and value
        std::string key = line.substr(0, div);
        std::string value = line.substr(div + 1);

        LOG_DEBUG("Got option " + key + "with value " + value);
        
        // Trim whitespace from key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        config[key] = value;
        
        line_num++;
    }
    
    cfile.close();
    return ErrorCodes::SUCCESS; // or appropriate success code
}

std::string Confparcer::get(const std::string& key, int* error_code = nullptr) const {
    // Provide default error code if null pointer is passed
    int dummy_error {0};
    int& e = error_code ? *error_code : dummy_error;

    if(config.size() < 1){
        LOG_WARN("No ARGS Loaded");
        return "";
    }
    try {
        LOG_DEBUG("Trying to get value from config");
        std::string value = config.at(key);
        e = ErrorCodes::SUCCESS;
        return value;
    } catch (const std::out_of_range&) {

         LOG_WARN("Unable to fetch key " +  key + " from config");

    e = ErrorCodes::NO_OPT_IN_CONFIG;
        return "";
    }
}
std::string Confparcer::getconfig() const {
    LOG_DEBUG("LOADING ENV");
    const char* env = std::getenv(HG_ENVKEY);
    
    std::filesystem::path base;
    
    if (env == nullptr) {
        LOG_WARN("Failed to load ENV");
        // Production path (uncomment for release)
        // base = "/var/HeavenGate/";
        
        // Development path
        base = "/root/Documents/HeavenGate/";
        LOG_DEBUG("LOADING base");
    } else {
        LOG_INFO("USING ENV PATH");
        base = env;
    }
    
    std::filesystem::path config = base / "config" / "default.ini";
    LOG_DEBUG("LOADING path");
    return config.string();
}
Confparcer::Confparcer(){
    parce();
    LOG_DEBUG("CONFPARCER PARSE CALLED");
}