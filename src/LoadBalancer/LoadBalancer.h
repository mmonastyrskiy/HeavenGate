/*
 * Filename: d:\HeavenGate\src\LoadBalancer\LoadBalancer.h
 * Path: d:\HeavenGate\src\LoadBalancer
 * Created Date: Saturday, November 8th 2025, 8:50:32 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */


#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <random>
#include <memory>
#include <map>
#include <string>
#include "../DataBus/DataBus.h"

struct BackendNode {
    std::string id;
    std::string host;
    int port;
    bool is_honeypot;
    float weight;
    std::atomic<int> current_clients {0};
    std::atomic<bool> is_healthy{true};
    std::chrono::steady_clock::time_point last_health_check;

    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_responses{0};
    std::atomic<uint64_t> failed_responses{0};
    std::atomic<uint64_t> total_response_time_ms{0};
    std::chrono::steady_clock::time_point last_request_time;

    BackendNode(const std::string& id, const std::string& host, int port,
                bool is_honeypot = false, float weight = 1.0f);
};

enum class RoutingStrategy {
    ROUND_ROBIN,
    LEAST_CONNECTIONS,
    IP_HASH,
    WEIGHTED
};

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
    size_t total_real_backends;
    size_t total_honeypot_backends;
    size_t healthy_real_backends;
    size_t healthy_honeypot_backends;
    int total_connections;

    uint64_t total_requests_processed;
    uint64_t requests_routed_to_real;
    uint64_t requests_routed_to_honeypot;
    uint64_t routing_errors;

    double requests_per_second;
    double average_routing_time_ms;
    std::chrono::steady_clock::time_point start_time;

    std::map<RoutingStrategy, uint64_t> strategy_usage;
    std::vector<BackendStats> backend_details;
};

struct PerformanceStats {
    std::atomic<uint64_t> total_routing_time_ns{0};
    std::atomic<uint64_t> total_routing_operations{0};
    std::atomic<uint64_t> health_check_operations{0};
    std::atomic<uint64_t> backend_selection_failures{0};
};

class LoadBalancer {
public:
    LoadBalancer(RoutingStrategy strategy);
    ~LoadBalancer();

    void add_backend(std::shared_ptr<BackendNode> server_ptr);
    std::shared_ptr<BackendNode> select_backend(bool is_malicious, const std::string& client_ip = "");
    void release_backend(const std::string& server_id);
    void start_health_checks(std::chrono::seconds interval = std::chrono::seconds(30));
    void stop();
    void set_routing_strategy(RoutingStrategy strategy);
    LoadBalancerStats get_stats() const;

private:
    RoutingStrategy strategy_;
    std::atomic<bool> running_{false};

    std::vector<std::shared_ptr<BackendNode>> real_backends_;
    std::vector<std::shared_ptr<BackendNode>> honeypot_backends_;
    mutable std::mutex backends_mutex_;

    std::atomic<size_t> round_robin_index_{0};
    std::thread health_check_thread_;

    SubscriptionId health_check_sub_;
    SubscriptionId classification_sub_;
    SubscriptionId response_sub_;

    LoadBalancerStats stats_;
    PerformanceStats performance_;

    std::shared_ptr<BackendNode> round_robin_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);
    std::shared_ptr<BackendNode> least_connections_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);
    std::shared_ptr<BackendNode> ip_hash_selection(const std::vector<std::shared_ptr<BackendNode>>& backends, const std::string& client_ip);
    std::shared_ptr<BackendNode> weighted_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);

    void perform_health_checks();
    bool check_server_health(const std::shared_ptr<BackendNode>& server);
    void handle_health_update(const Event& event);
    void handle_classification(const Event& event);
    void handle_response_metrics(const Event& event);

    void mark_request_success(const std::string& server_id, std::chrono::milliseconds response_time);
    void mark_request_failure(const std::string& server_id);
    static std::string strategy_to_string(RoutingStrategy strategy);
};
