#pragma once
#include <string>


#ifndef LOGGER_H
#define LOGGER_H

#define LOG_ERROR(msg) logger::Logger::err(msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) logger::Logger::fatal(msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg) logger::Logger::debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) logger::Logger::info(msg)
#define LOG_WARN(msg) logger::Logger::warn(msg)

namespace logger {

class Logger
{
public:
    Logger();
    static void info(const std::string&);
    static void warn(const std::string&);
    static void err(const std::string&, const char * file=__FILE__, int line=__LINE__);
    static void fatal(const std::string&,const char * file=__FILE__, int line=__LINE__);
    static void debug(const std::string&,const char * file=__FILE__, int line=__LINE__);
    static void writelog(const std::string&);

private:
    static std::string getCurrentTimeISO();

};
}

#endif // LOGGER_H
