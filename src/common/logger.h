#pragma once
#include <string>
#ifndef LOGGER_H
#define LOGGER_H

namespace logger{

 static inline bool WRITE_TO_FILE = [](){
        return Confparcer::SETTING<bool>("ENABLE_LOG_FILE", "0");
    }();


 static inline std::string LOG_PATH = [](){
        return Confparcer::SETTING<std::string>("LOG_FILE", ".");
    }();
class Logger
{
public:

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
