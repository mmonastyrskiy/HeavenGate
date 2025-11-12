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
#include <algorithm> // for std::remove
#include <cctype>    // for std::isspace
#include <cstdlib>
#include <filesystem>

Confparcer& Confparcer::the() {
    static Confparcer c;
    return c;
}

int Confparcer::parce() {
    std::string path = getconfig();
    std::ifstream cfile(path); 
    
    if (!cfile.is_open()) {
        LOG_FATAL("Failed to open config file, please check the path");
        return ErrorCodes::CONFIG_NOT_OPENED;
    }
    
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
    int dummy_error;
    int& e = error_code ? *error_code : dummy_error;
    
    try {
        std::string value = config.at(key);
        e = ErrorCodes::SUCCESS;
        return value;
    } catch (const std::out_of_range&) {

         LOG_WARN("Unable to fetch key " +  key + " from config");

    e = ErrorCodes::NO_OPT_IN_CONFIG;
        return "";
    }
}
std::string Confparcer::getconfig() const{
const char* env;

env = std::getenv(HG_ENVKEY);
if (env == nullptr){
std::filesystem::path base = "/var/HeavenGate"; //TODO: Has to be created
}
std::filesystem::path base = env;
std::filesystem::path config = base /= "config";
config += "default.ini";
return config.string();


}