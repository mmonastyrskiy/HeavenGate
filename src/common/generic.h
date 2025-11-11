#include "logger.h"
#ifndef ISLINUX
    #if defined(__linux__) || defined(linux) || defined(__linux)
        #define ISLINUX 1
    #else
        #define ISLINUX 0
    #endif
#endif

#define TODO() do{ logger::Logger::fatal("TODO REACHED");} while(0)
