/*
 * Filename: d:\HeavenGate\src\LoadBalancer\LoadBalancer.h
 * Path: d:\HeavenGate\src\LoadBalancer
 * Created Date: Saturday, November 9th 2025
 * Author: mmonastyrskiy
 */

#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "../../thirdparty/asio/include/asio.hpp"
#include "../DataBus/DataBus.h"

class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    using Ptr = std::shared_ptr<ClientConnection>;
    
    std::string client_ip;
    std::string client_id;
    asio::ip::tcp::socket socket;
    std::shared_ptr<asio::ip::tcp::socket> backend_socket;
    std::atomic<bool> is_malicious{false};
    std::atomic<bool> active{true};
    
    ClientConnection(asio::io_context& io_context, const std::string& ip);
    void start();
    void close();
};

class BackendNode {
public:
    using Ptr = std::shared_ptr<BackendNode>;
    
    std::string id;
    std::string host;
    int port;
    bool is_honeypot;
    float weight;
    std::atomic<bool> is_healthy{true};
    std::atomic<int> current_clients{0};
    std::atomic<long> total_requests{0};
    std::chrono::steady_clock::time_point last_request_time;
    std::chrono::steady_clock::time_point last_health_check;

    BackendNode(const std::string& id, const std::string& host, int port,
                bool is_honeypot = false, float weight = 1.0f);
};

enum class RoutingStrategy {
    ROUND_ROBIN,
    LEAST_CONNECTIONS,
    IP_HASH,
    WEIGHTED
};

struct LoadBalancerStats {
    size_t total_requests_processed{0};
    size_t requests_routed_to_real{0};
    size_t requests_routed_to_honeypot{0};
    size_t routing_errors{0};
    size_t total_real_backends{0};
    size_t total_honeypot_backends{0};
    size_t healthy_real_backends{0};
    size_t healthy_honeypot_backends{0};
    size_t total_connections{0};
    std::chrono::steady_clock::time_point start_time;
    std::unordered_map<RoutingStrategy, size_t> strategy_usage;
};

struct PerformanceMetrics {
    std::atomic<long long> total_routing_time_ns{0};
    std::atomic<long> total_routing_operations{0};
    std::atomic<long> backend_selection_failures{0};
};

class LoadBalancer {
public:
    LoadBalancer(RoutingStrategy strategy = RoutingStrategy::ROUND_ROBIN);
    ~LoadBalancer();

    void start(int port = 80);
    void stop();
    
    void add_backend(std::shared_ptr<BackendNode> server_ptr);
    void set_routing_strategy(RoutingStrategy strategy);
    
    LoadBalancerStats get_stats() const;
    const PerformanceMetrics& get_performance_metrics() const;
    
    static std::string strategy_to_string(RoutingStrategy strategy);

private:
    RoutingStrategy strategy_;
    std::atomic<bool> running_{false};
    
    std::vector<std::shared_ptr<BackendNode>> real_backends_;
    std::vector<std::shared_ptr<BackendNode>> honeypot_backends_;
    mutable std::mutex backends_mutex_;
    
    std::unordered_map<std::string, std::shared_ptr<BackendNode>> client_backend_mapping_;
    mutable std::mutex mapping_mutex_;
    
    LoadBalancerStats stats_;
    PerformanceMetrics performance_;
    
    std::atomic<size_t> round_robin_index_{0};
    
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread server_thread_;
    std::thread health_check_thread_;
    
    // DataBus subscriptions
    SubscriptionId health_check_sub_;
    SubscriptionId classification_sub_;
    SubscriptionId response_sub_;

    void start_accept();
    void handle_accept(ClientConnection::Ptr client, const asio::error_code& error);
    
    std::shared_ptr<BackendNode> select_backend(bool is_malicious, const std::string& client_ip);
    std::shared_ptr<BackendNode> get_assigned_backend(const std::string& client_ip);
    void assign_backend_to_client(const std::string& client_ip, std::shared_ptr<BackendNode> backend);
    
    void release_backend(const std::string& server_id);
    
    // Selection strategies
    std::shared_ptr<BackendNode> round_robin_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);
    std::shared_ptr<BackendNode> least_connections_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);
    std::shared_ptr<BackendNode> ip_hash_selection(const std::vector<std::shared_ptr<BackendNode>>& backends, const std::string& client_ip);
    std::shared_ptr<BackendNode> weighted_selection(const std::vector<std::shared_ptr<BackendNode>>& backends);
    
    // Health checking
    void start_health_checks(std::chrono::seconds interval = std::chrono::seconds(30));
    void perform_health_checks();
    bool check_server_health(const std::shared_ptr<BackendNode>& server);
    
    // Event handlers
    void handle_health_update(const Event& event);
    void handle_classification(const Event& event);
    void handle_response_metrics(const Event& event);
    
    void mark_request_success(const std::string& server_id, std::chrono::milliseconds response_time);
    void mark_request_failure(const std::string& server_id);
    
    // Proxy functionality
    void proxy_to_backend(ClientConnection::Ptr client, std::shared_ptr<BackendNode> backend);
    void handle_client_request(ClientConnection::Ptr client);
    void read_from_client(ClientConnection::Ptr client);
    void read_from_backend(ClientConnection::Ptr client);
};

#endif // LOADBALANCER_H