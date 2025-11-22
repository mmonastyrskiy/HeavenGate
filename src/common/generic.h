/*
 * Filename: d:\HeavenGate\src\common\generic.h
 * Path: d:\HeavenGate\src\common
 * Created Date: Tuesday, November 11th 2025, 8:38:24 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once
#include "logger.h"
#include "../LoadBalancer/LoadBalancer.h"

#ifndef ISLINUX
    #if defined(__linux__) || defined(linux) || defined(__linux)
        #define ISLINUX 1
    #else
        #define ISLINUX 0
    #endif
#endif

#define TODO() do{ LOG_FATAL("TODO REACHED");} while(0)
#define VERIFY_NOT_REACHED() do{ LOG_FATAL("UNEXPECTED REACHED");} while(0)

#define NO_COPY(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

// Запрет перемещения
#define NO_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete;


#define KDEBUG 1 // TODO: CHANGE ON RELEASE

#if not defined KDEBUG && defined ALLGOOD
LOG_FATAL("THIS IS IMPOSIBBLE COMPILE A DEBUG BUILD");
#endif


