/*
 * Filename: d:\HeavenGate\src\AppManager\AppComponent.h
 * Path: d:\HeavenGate\src\AppManager
 * Created Date: Tuesday, November 11th 2025, 8:38:24 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once
#include <filesystem>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <cstring>
#include "../common/generic.h"
#include "../common/logger.h"
#if ISLINUX
#include <unistd.h>
#include <sys/wait.h>
#endif

enum AppComponentType {
    HG_DASHBOARD
};

class AppComponent
{
private:
    std::filesystem::path path;
    size_t pid = 0;
    int timeout_seconds = 10;

public:
    size_t proc_pid;
    AppComponentType type;
    std::string name;

    inline AppComponent(AppComponentType comp_type);
    inline ~AppComponent();

    inline bool run();
    inline bool stop();
};

inline AppComponent::AppComponent(AppComponentType comp_type)
{
    switch(comp_type) {
        case AppComponentType::HG_DASHBOARD: {
            path /= "go-apps";
            path += "dashboard";
            name = "Dashboard";
            this->type = comp_type;
            break;
        }
    }
    run();
}

inline AppComponent::~AppComponent()
{
    stop();
}

inline bool AppComponent::run() {
#if ISLINUX
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl(path.c_str(), "dashboard", NULL);
        logger::Logger::err("Failed to run " + name);
        return false;
    }
    else if (pid > 0) {
        // Parent process
        int status;
        proc_pid = pid;
        this->pid = pid;
        waitpid(pid, &status, 0); 
        if (status != 0) {
            logger::Logger::err("Service " + name + " Exited with: " + std::to_string(status) + " Code. \n Restarting!");
            run(); // Recursive restart
        }
        else {
            logger::Logger::info("Service " + name + " Stopped!");
        }
        return true;
    }
    else {
        logger::Logger::err("Failed to run " + name + " Fork failed");
        return false;
    }
#else
    logger::Logger::err("run() not implemented for this platform");
    // TODO: Implement for other platforms
    TODO();
    return false;
#endif
}

inline bool AppComponent::stop() {
#if ISLINUX
    if (pid == 0) {
        logger::Logger::warn("Process " + name + " not running");
        return true;
    }

    // Check if process exists
    if (kill(pid, 0) != 0) {
        if (errno == ESRCH) {
            logger::Logger::warn("Process " + name + "[ " + std::to_string(pid) + " ]" + " Not found! Unable to stop!");
        } else {
            logger::Logger::warn("Cannot access process of " + name + "[" + std::to_string(pid) + "]" + " Reason: " + strerror(errno));
        }
        return false;
    }

    // Send SIGTERM for graceful shutdown
    logger::Logger::info("Sending SIGTERM to process " + name + "[" + std::to_string(pid) + "]");
    if (kill(pid, SIGTERM) != 0) {
        logger::Logger::warn("Failed to send SIGTERM: " + std::string(strerror(errno)));
        return false;
    }

    // Wait for process termination
    for (int i = 0; i < timeout_seconds; i++) {
        sleep(1);
        if (kill(pid, 0) != 0) {
            if (errno == ESRCH) {
                logger::Logger::info("Process " + name + "[" + std::to_string(pid) + "]" + " terminated gracefully.");
                pid = 0;
                proc_pid = 0;
                return true;
            }
        }
    }

    // Force kill if still running
    logger::Logger::warn("Process " + name + "[" + std::to_string(pid) + "]" + " did not terminate, sending SIGKILL");
    if (kill(pid, SIGKILL) != 0) {
        logger::Logger::err("Failed to send SIGKILL: " + std::string(strerror(errno)));
        return false;
    }
    
    sleep(1); // Give time for SIGKILL to process
    
    if (kill(pid, 0) == 0) {
        logger::Logger::err("Process " + name + "[" + std::to_string(pid) + "]" + " still running after SIGKILL!");
        return false;
    }
    
    logger::Logger::info("Process " + name + "[" + std::to_string(pid) + "]" + " killed successfully.");
    pid = 0;
    proc_pid = 0;
    return true;
#else
    logger::Logger::err("stop() not implemented for this platform");
    // TODO: Implement for other platforms
    TODO();
    return false;
#endif
}