#include <string>
#include <atomic>
#include <DataBus.h>
#include "LoadBalancer.h"
struct BackendStatsSnapshot {
        std::string server_id;
        std::string host;
        int port;
        bool is_honeypot;
        bool healthy;
        int current_connections;
        int weight;
        
        uint64_t total_requests;
        uint64_t successful_responses;
        uint64_t failed_responses;
        double success_rate;
        double average_response_time_ms;
        uint64_t uptime_seconds;
        std::chrono::steady_clock::time_point last_request_time;
    
};
