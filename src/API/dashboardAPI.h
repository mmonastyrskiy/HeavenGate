/*
 * Filename: d:\HeavenGate\src\API\dashboardAPI.h
 * Path: d:\HeavenGate\src\API
 * Created Date: Saturday, November 8th 2025, 8:50:32 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#ifndef DASHBOARDAPI_HPP
#define DASHBOARDAPI_HPP

#include <string>
#include "../common/Confparcer.h"

class DashboardAPI {
public:
    int err{0};

    
    static inline const char* HOST = [](){
        return Confparcer::SETTING<const char*>("DASHBOARD_HOST", "127.0.0.1");
    }();
    static inline const size_t PORT = [](){
        return Confparcer::SETTING<size_t>("DASHBOARD_PORT",8081);
    }();

    // Singleton instance
    static DashboardAPI& the();

    // Main API method
    std::string callUserRegistered(const std::string& client_ip,
                                   const std::string& server_id,
                                   bool is_malicious,
                                   int* err = nullptr);

    DashboardAPI() = default;
    ~DashboardAPI() = default;

  std::string baseUrl = std::string("http://") + HOST + ":" + std::to_string(PORT) + "/api";

private:
    // Prevent copying
    DashboardAPI(const DashboardAPI&) = delete;
    DashboardAPI& operator=(const DashboardAPI&) = delete;
};

#endif // DASHBOARDAPI_HPP
