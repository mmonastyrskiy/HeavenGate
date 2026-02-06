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
#include <vector>
#include "../common/generic.h"
#include "../common/logger.h"
#include "../common/Confparcer.h"
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
            if(!(Confparcer::SETTING<std::string>("DASHBOARD_HOST","127.0.0.1") == "127.0.0.1")){
                LOG_FATAL("THE DASHBOARD IS NOT RUNNING IN LOCAL MODE");
                break;
            }
            // Python dashboard path - use run_dashboard.py wrapper for better path resolution
            path = "../Dashboard/run_dashboard.py";
            name = "dashboard";
            this->type = comp_type;
            break;
        }
        default:
        VERIFY_NOT_REACHED();

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
        if (exit_code != 0 || exit_code != 15 || exit_code != 9) {
            LOG_ERROR("Service " + name + " Exited with: " + std::to_string(exit_code) + " Code. Restarting!");
            // Перезапускаем в том же потоке мониторинга
            is_running = false;
            run();
        } else {
            LOG_INFO("Service " + name + " Stopped gracefully.");
            is_running = false;
        }
    } else if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        if(signal != 15 || signal != 9){
            //FIXME: This fix does not work for BUG-5 for some reason
        LOG_ERROR("Service " + name + " Terminated by signal: " + std::to_string(signal));
        }
        else{LOG_INFO("Service " + name + " Terminated by signal: " + std::to_string(signal));}
        is_running = false;
    }
#endif
}

bool findPythonPath(std::string& path) {
    // Проверяем доступные пути к Python
    std::vector<std::string> pythonPaths = {
        "python3",
        "python",
        "/usr/bin/python3",
        "/usr/bin/python",
        "/usr/local/bin/python3",
        "/usr/local/bin/python"
    };
    
    for (const auto& pythonPath : pythonPaths) {
        std::string command = pythonPath + " --version > /dev/null 2>&1";
        if (system(command.c_str()) == 0) {
            path = pythonPath;
            return true;
        }
    }
    
    return false;
}

inline bool AppComponent::run() {
    std::string py;
    if (!findPythonPath(py)) {
        LOG_ERROR("Python not found. Cannot start " + name);
        return false;
    }
#if ISLINUX
    if (is_running.load()) {
        LOG_WARN("Service " + name + " is already running");
        return true;
    }

    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Дочерний процесс
        // Get absolute path to dashboard script
        std::filesystem::path script_path = path;
        if (!script_path.is_absolute()) {
            // Try multiple locations:
            // 1. Relative to current working directory
            std::filesystem::path cwd_path = std::filesystem::current_path() / script_path;
            if (std::filesystem::exists(cwd_path)) {
                script_path = cwd_path;
            } else {
                // 2. Relative to executable location (assuming we're in build/bin)
                std::filesystem::path exe_path = std::filesystem::path("../../Dashboard/run_dashboard.py");
                if (std::filesystem::exists(exe_path)) {
                    script_path = std::filesystem::absolute(exe_path);
                } else {
                    // 3. Try from project root
                    std::filesystem::path root_path = std::filesystem::path("../Dashboard/run_dashboard.py");
                    if (std::filesystem::exists(root_path)) {
                        script_path = std::filesystem::absolute(root_path);
                    } else {
                        // Last resort: use original path and hope it works
                        script_path = std::filesystem::absolute(script_path);
                    }
                }
            }
        }
        
        std::string script_str = script_path.string();
        
        // Change to Dashboard directory for proper static file serving
        std::filesystem::path dashboard_dir = script_path.parent_path();
        if (std::filesystem::exists(dashboard_dir)) {
            std::filesystem::current_path(dashboard_dir);
        }
        
        // Подготовка аргументов для execv: python3 script_path
        char* argv[] = {
            const_cast<char*>(py.c_str()),
            const_cast<char*>(script_str.c_str()),
            NULL
        };
        
        execv(py.c_str(), argv);
        
        // Если execv вернул управление - ошибка
        LOG_ERROR("Failed to run " + name + ": " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }
    else if (child_pid > 0) {
        // Родительский процесс
        proc_pid = child_pid;
        this->pid = child_pid;
        is_running = true;
        
        LOG_INFO("Service " + name + " started with PID: " + std::to_string(child_pid));
        
        // Запускаем мониторинг в отдельном потоке
        if (monitor_thread && monitor_thread->joinable()) {
            monitor_thread->join();
        }
        monitor_thread = std::make_unique<std::thread>(&AppComponent::monitor_process, this);
        
        return true;
    }
    else {
        LOG_ERROR("Failed to run " + name + " - Fork failed: " + std::string(strerror(errno)));
        return false;
    }
#else
    LOG_ERROR("run() not implemented for this platform");
    // TODO: Implement for other platforms
    TODO();
    return false;
#endif
}

inline bool AppComponent::stop() {
#if ISLINUX
    if (!is_running.load() || pid == 0) {
        LOG_WARN("Process " + name + " not running");
        return true;
    }

    // Check if process exists
    if (kill(pid, 0) != 0) {
        if (errno == ESRCH) {
            LOG_WARN("Process " + name + "[ " + std::to_string(pid) + " ]" + " Not found! Unable to stop!");
            is_running = false;
            pid = 0;
            proc_pid = 0;
        } else {
            LOG_WARN("Cannot access process of " + name + "[" + std::to_string(pid) + "]" + " Reason: " + strerror(errno));
        }
        return false;
    }

    // Send SIGTERM for graceful shutdown
    LOG_INFO("Sending SIGTERM to process " + name + "[" + std::to_string(pid) + "]");
    if (kill(pid, SIGTERM) != 0) {
        LOG_WARN("Failed to send SIGTERM: " + std::string(strerror(errno)));
        return false;
    }

    // Wait for process termination
    for (int i = 0; i < timeout_seconds; i++) {
        sleep(1);
        if (kill(pid, 0) != 0) {
            if (errno == ESRCH) {
                LOG_INFO("Process " + name + "[" + std::to_string(pid) + "]" + " terminated gracefully.");
                is_running = false;
                pid = 0;
                proc_pid = 0;
                return true;
            }
        }
    }

    // Force kill if still running
    LOG_WARN("Process " + name + "[" + std::to_string(pid) + "]" + " did not terminate, sending SIGKILL");
    if (kill(pid, SIGKILL) != 0) {
        LOG_ERROR("Failed to send SIGKILL: " + std::string(strerror(errno)));
        return false;
    }
    
    sleep(1); // Give time for SIGKILL to process
    
    if (kill(pid, 0) == 0) {
        LOG_ERROR("Process " + name + "[" + std::to_string(pid) + "]" + " still running after SIGKILL!");
        return false;
    }
    
    LOG_INFO("Process " + name + "[" + std::to_string(pid) + "]" + " killed successfully.");
    is_running = false;
    pid = 0;
    proc_pid = 0;
    return true;
#else
    LOG_ERROR("stop() not implemented for this platform");
    // TODO: Implement for other platforms
    TODO();
    return false;
#endif
}