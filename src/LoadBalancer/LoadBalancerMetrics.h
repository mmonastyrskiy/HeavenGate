#include <string>
#include <atomic>
#include "../DataBus/DataBus.h"
#include "LoadBalancer.h"
struct BackendStats {
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
struct LoadBalancerStats {
        // Основная статистика
        size_t total_real_backends;
        size_t total_honeypot_backends;
        size_t healthy_real_backends;
        size_t healthy_honeypot_backends;
        int total_connections;
        
        // Статистика производительности
        uint64_t total_requests_processed;
        uint64_t requests_routed_to_real;
        uint64_t requests_routed_to_honeypot;
        uint64_t routing_errors;
        
        // Временная статистика
        double requests_per_second;
        double average_routing_time_ms;
        std::chrono::steady_clock::time_point start_time;
        
        // Статистика по стратегиям
        std::map<RoutingStrategy, uint64_t> strategy_usage;
        
        // Детальная статистика по бэкендам
        std::vector<BackendStats> backend_details;

};
struct PerformanceStats {
        std::atomic<uint64_t> total_routing_time_ns{0};
        std::atomic<uint64_t> total_routing_operations{0};
        std::atomic<uint64_t> health_check_operations{0};
        std::atomic<uint64_t> backend_selection_failures{0};
    };