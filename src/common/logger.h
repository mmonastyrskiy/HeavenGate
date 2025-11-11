#pragma once
#include <string>


#ifndef LOGGER_H
#define LOGGER_H

namespace logger {

class Logger
{
public:

    static bool WRITE_TO_FILE() {
        static bool value = Confparcer::SETTING<bool>("ENABLE_LOG_FILE", false);
        return value;
    }
    
    static const std::string& LOG_PATH() {
        static std::string path = Confparcer::SETTING<std::string>("LOG_PATH", ".");
        return path;
    }
    Logger();
    static void info(const std::string&);
    static void warn(const std::string&);
    static void err(const std::string&);
    static void fatal(const std::string&);
    static void debug(const std::string&);
    static void writelog(const std::string&);

private:
    static std::string getCurrentTimeISO();

};
}

#endif // LOGGER_H
