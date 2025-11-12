#pragma once
#include <string>


#ifndef LOGGER_H
#define LOGGER_H

namespace logger {

class Logger
{
public:
    Logger();
    static void info(const std::string&);
    static void warn(const std::string&);
    static void err(const std::string&, const char * file, int line);
    static void fatal(const std::string&,const char * file, int line);
    static void debug(const std::string&,const char * file, int line);
    static void writelog(const std::string&);

private:
    static std::string getCurrentTimeISO();

};
}

#endif // LOGGER_H
