#include "logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include "../../include/colorText.h"

namespace logger {

Logger::Logger() = default;

std::string Logger::getCurrentTimeISO() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

void Logger::info(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp << " [INFO] ";
    color::print::info() << msg;
    std::cout << std::endl;
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [INFO] " + msg);
    }
}

void Logger::warn(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp << " [WARN] ";
    color::print::warning() << msg;
    std::cout << std::endl;
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [WARN] " + msg);
    }
}

void Logger::err(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp << " [ERROR] ";
    color::print::error() << msg;
    std::cout << std::endl;
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [ERROR] " + msg);
    }
}

void Logger::fatal(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp << " [FATAL] ";
    color::print::magenta().bold() << msg;
    std::cout << std::endl;
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [FATAL] " + msg);
    }
}

void Logger::writelog(const std::string& towrite) {
    std::ofstream file(LOG_PATH  + "/application.log", std::ios::app);
    if (file.is_open()) {
        file << towrite << std::endl;
        file.close();
    }
}

} 