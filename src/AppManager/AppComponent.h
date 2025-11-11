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
#include <thread>
#include <atomic>
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
    std::atomic<bool> is_running{false};
    std::unique_ptr<std::thread> monitor_thread;

    void monitor_process(); // Мониторинг процесса в отдельном потоке

public:
    size_t proc_pid;
    AppComponentType type;
    std::string name;

    // Конструкторы и операторы
    inline AppComponent(AppComponentType comp_type);
    inline ~AppComponent();
    
    // Запрещаем копирование
    AppComponent(const AppComponent&) = delete;
    AppComponent& operator=(const AppComponent&) = delete;
    
    // Разрешаем перемещение
    inline AppComponent(AppComponent&& other) noexcept;
    inline AppComponent& operator=(AppComponent&& other) noexcept;

    inline bool run();
    inline bool stop();
    inline bool isRunning() const { return is_running.load(); }
};

inline AppComponent::AppComponent(AppComponentType comp_type)
{
    switch(comp_type) {
        case AppComponentType::HG_DASHBOARD: {
            path /= "go-apps";;
            name = "dashboard";
            this->type = comp_type;
            break;
        }
    }
}

// Конструктор перемещения
inline AppComponent::AppComponent(AppComponent&& other) noexcept
    : path(std::move(other.path))
    , pid(other.pid)
    , timeout_seconds(other.timeout_seconds)
    , is_running(other.is_running.load())
    , monitor_thread(std::move(other.monitor_thread))
    , proc_pid(other.proc_pid)
    , type(other.type)
    , name(std::move(other.name))
{
    other.pid = 0;
    other.proc_pid = 0;
    other.is_running = false;
}

// Оператор перемещения
inline AppComponent& AppComponent::operator=(AppComponent&& other) noexcept {
    if (this != &other) {
        // Останавливаем текущий процесс если запущен
        stop();
        if (monitor_thread && monitor_thread->joinable()) {
            monitor_thread->join();
        }
        
        path = std::move(other.path);
        pid = other.pid;
        timeout_seconds = other.timeout_seconds;
        is_running = other.is_running.load();
        monitor_thread = std::move(other.monitor_thread);
        proc_pid = other.proc_pid;
        type = other.type;
        name = std::move(other.name);
        
        other.pid = 0;
        other.proc_pid = 0;
        other.is_running = false;
    }
    return *this;
}

inline AppComponent::~AppComponent()
{
    stop();
    // Ждем завершения мониторингового потока если он запущен
    if (monitor_thread && monitor_thread->joinable()) {
        monitor_thread->join();
    }
}

inline void AppComponent::monitor_process() {
#if ISLINUX
    int status;
    pid_t monitored_pid = this->pid;
    
    // Ждем завершения процесса
    waitpid(monitored_pid, &status, 0);
    
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            logger::Logger::err("Service " + name + " Exited with: " + std::to_string(exit_code) + " Code. Restarting!");
            // Перезапускаем в том же потоке мониторинга
            is_running = false;
            run();
        } else {
            logger::Logger::info("Service " + name + " Stopped gracefully.");
            is_running = false;
        }
    } else if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        logger::Logger::err("Service " + name + " Terminated by signal: " + std::to_string(signal));
        is_running = false;
    }
#endif
}

inline bool AppComponent::run() {
#if ISLINUX
    if (is_running.load()) {
        logger::Logger::warn("Service " + name + " is already running");
        return true;
    }

    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Дочерний процесс
        execl(path.c_str(), name.c_str(), NULL);
        // Если execl вернул управление - ошибка
        logger::Logger::err("Failed to run " + name + ": " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }
    else if (child_pid > 0) {
        // Родительский процесс
        proc_pid = child_pid;
        this->pid = child_pid;
        is_running = true;
        
        logger::Logger::info("Service " + name + " started with PID: " + std::to_string(child_pid));
        
        // Запускаем мониторинг в отдельном потоке
        if (monitor_thread && monitor_thread->joinable()) {
            monitor_thread->join();
        }
        monitor_thread = std::make_unique<std::thread>(&AppComponent::monitor_process, this);
        
        return true;
    }
    else {
        logger::Logger::err("Failed to run " + name + " - Fork failed: " + std::string(strerror(errno)));
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
    if (!is_running.load() || pid == 0) {
        logger::Logger::warn("Process " + name + " not running");
        return true;
    }

    // Check if process exists
    if (kill(pid, 0) != 0) {
        if (errno == ESRCH) {
            logger::Logger::warn("Process " + name + "[ " + std::to_string(pid) + " ]" + " Not found! Unable to stop!");
            is_running = false;
            pid = 0;
            proc_pid = 0;
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
                is_running = false;
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
    is_running = false;
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