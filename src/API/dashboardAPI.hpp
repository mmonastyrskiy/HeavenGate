#ifndef DASHBOARDAPI_HPP
#define DASHBOARDAPI_HPP

#include <string>

class DashboardAPI {
public:
    int err{0};
    
    // Singleton instance
    static DashboardAPI& the();
    
    // Main API method
    std::string callUserRegistered(const std::string& client_ip, 
                                  const std::string& server_id,
                                  bool is_malicious,
                                  int* err = nullptr);
    
    DashboardAPI() = default;
    ~DashboardAPI() = default;

    static constexpr const char* baseUrl = "http://127.0.0.1:8081";

private:
    // Prevent copying
    DashboardAPI(const DashboardAPI&) = delete;
    DashboardAPI& operator=(const DashboardAPI&) = delete;
};

#endif // DASHBOARDAPI_HPP