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
#ifndef ISLINUX
    #if defined(__linux__) || defined(linux) || defined(__linux)
        #define ISLINUX 1
    #else
        #define ISLINUX 0
    #endif
#endif

#define TODO() do{ LOG_FATAL("TODO REACHED");} while(0)
#define VERIFY_NOT_REACHED() do{ LOG_FATAL("UNEXPECTED REACHED");} while(0)

