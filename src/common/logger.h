#include <string>
#ifndef LOGGER_H
#define LOGGER_H

namespace logger{
static const bool WRITE_TO_FILE = false;
static const std::string LOG_PATH = ".";
class Logger
{
public:

    Logger();
    static void info(const std::string&);
    static void warn(const std::string&);
    static void err(const std::string&);
    static void fatal(const std::string&);
    static void writelog(const std::string&);

private:
    static std::string getCurrentTimeISO();

};
}

#endif // LOGGER_H
