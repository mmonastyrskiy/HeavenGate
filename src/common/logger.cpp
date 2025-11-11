
#include "logger.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "../../include/colorText.h"
#include "Confparcer.h"

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
    std::cout << timestamp;
    auto p = color::print::info() << " [INFO] " << msg;
    p.println();
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [INFO] " + msg);
    }
}

void Logger::warn(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp ;
    auto p = color::print::warning() <<" [WARN] " << msg;
    p.println();
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [WARN] " + msg);
    }
}

void Logger::err(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp;
    auto p = color::print::error() << " [ERROR] " + static_cast<std::string>( __FILE__) + static_cast<char>(__LINE__) << " " << msg;
    p.println();
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [ERROR]  " + __FILE__ + static_cast<char>(__LINE__) + msg);
    }
}

void Logger::fatal(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp;
    auto p = color::print::magenta().bold() << " [FATAL] " + static_cast<std::string>( __FILE__) + static_cast<char>(__LINE__) <<" "<< msg;
    p.println();

    
    
    if(WRITE_TO_FILE) {
        writelog(timestamp + " [FATAL] " +  + __FILE__ + static_cast<char>(__LINE__) + msg);
    }
    std::runtime_error("Fatal called " +static_cast<std::string>( __FILE__) + static_cast<char>(__LINE__));

}

void Logger::debug(const std::string& msg) {
    std::string timestamp = getCurrentTimeISO();
    std::cout << timestamp;
    auto p = color::print::green().bold() << " [DEBUG] " + static_cast<std::string>( __FILE__) + static_cast<char>(__LINE__) << " " << msg;
    p.println();
}

void Logger::writelog(const std::string& towrite) {
    std::ofstream file(LOG_PATH()  + "/application.log", std::ios::app);
    if (file.is_open()) {
        file << towrite << std::endl;
        file.close();
    }
}

} 