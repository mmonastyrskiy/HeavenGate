#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include "LoadBalancer.h"
#include "LoadBalancerMetrics.h"
#include "../DataBus/DataBus.h"
#include "../DataBus/SubscriptionID.h"
#include "../DataBus/BusEvent.h"
#include "../DataBus/DataBus.h"

#include "../../thirdparty/json.hpp"

// Forward declarations or include proper headers


struct LoadBalancerStats {
    std::chrono::steady_clock::time_point start_time;
    // Add other stats members as needed
};

struct PerformanceStats {
    // Define performance stats structure
};

class LoadBalancer
{
private:
    LoadBalancer() = delete; // Remove private default constructor

public:
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
                   bool is_honeypot = false, float weight = 1.0f)
            : id(id), host(host), port(port), is_honeypot(is_honeypot), weight(weight) {}
    };

    LoadBalancer(RoutingStrategy strategy = RoutingStrategy::ROUND_ROBIN)
        : strategy_(strategy), running_(false) {
        stats_.start_time = std::chrono::steady_clock::now();
        
        // Регистрация в шине данных
        SubscriptionId health_check_sub_ = DataBus::instance().subscribe(
            BusEventType::SERVICE_HEALTH_UPDATE,
            [this](const Event& event) {
                this->handle_health_update(event);
            }
        );

        classification_sub_ = DataBus::instance().subscribe(
            BusEventType::REQUEST_CLASSIFIED,
            [this](const Event &event) {
                this->handle_classification(event);
            });

        response_sub_ = DataBus::instance().subscribe(
            BusEventType::REQUEST_PROCESSED,
            [this](const Event& event) {
                this->handle_response_metrics(event);
            }
        );
    }

    ~LoadBalancer() {
        stop();
        DataBus::instance().unsubscribe(health_check_sub_);
        DataBus::instance().unsubscribe(classification_sub_);
        DataBus::instance().unsubscribe(response_sub_);
    }

    void add_backend(const BackendNode& server) {
        std::lock_guard<std::mutex> lock(backends_mutex_);
        auto server_ptr = std::make_shared<BackendNode>(server);
        
        if (server.is_honeypot) {
            honeypot_backends_.push_back(server_ptr);
        } else {
            real_backends_.push_back(server_ptr);
        }

        DataBus::instance().publish(
            BusEventType::SERVICE_REGISTERED,
            (std::string) "load_balancer",
            {
                {"server_id", server.id},
                {"host", server.host},
                {"port", server.port},
                {"is_honeypot", server.is_honeypot},
                {"weight", server.weight}
            }
        );
    }

    void stop() {
        running_ = false;
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
    }

     std::shared_ptr<BackendNode> select_backend(bool is_malicious, const std::string& client_ip = "") {
        auto start_time = std::chrono::steady_clock::now();
        
        auto& backends = is_malicious ? honeypot_backends_ : real_backends_;
        
        if (backends.empty()) {
            stats_.routing_errors++;
            performance_.backend_selection_failures++;
            return nullptr;
        }
        
        std::lock_guard<std::mutex> lock(backends_mutex_);
        
        // Фильтруем только здоровые бэкенды
        std::vector<std::shared_ptr<BackendNode>> healthy_backends;
        for (const auto& backend : backends) {
            if (backend->healthy.load()) {
                healthy_backends.push_back(backend);
            }
        }
        
        if (healthy_backends.empty()) {
            stats_.routing_errors++;
            performance_.backend_selection_failures++;
            return nullptr;
        }
        
        std::shared_ptr<BackendNode> selected;
        
        switch (strategy_) {
            case RoutingStrategy::ROUND_ROBIN:
                selected = round_robin_selection(healthy_backends);
                break;
                
            case RoutingStrategy::LEAST_CONNECTIONS:
                selected = least_connections_selection(healthy_backends);
                break;
                
            case RoutingStrategy::IP_HASH:
                selected = ip_hash_selection(healthy_backends, client_ip);
                break;
                
            case RoutingStrategy::WEIGHTED:
                selected = weighted_selection(healthy_backends);
                break;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto routing_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        
        performance_.total_routing_time_ns += routing_time_ns;
        performance_.total_routing_operations++;
        
        if (selected) {
            selected->current_connections++;
            selected->total_requests++;
            selected->last_request_time = std::chrono::steady_clock::now();
            
            stats_.total_requests_processed++;
            if (is_malicious) {
                stats_.requests_routed_to_honeypot++;
            } else {
                stats_.requests_routed_to_real++;
            }
            
            stats_.strategy_usage[strategy_]++;
            
            DataBus::instance().publish(
                EventType::REQUEST_ROUTED,
                "load_balancer",
                {
                    {"client_ip", client_ip},
                    {"server_id", selected->id},
                    {"is_malicious", is_malicious},
                    {"strategy", static_cast<int>(strategy_)},
                    {"current_connections", selected->current_connections.load()},
                    {"routing_time_ns", routing_time_ns},
                    {"total_requests", selected->total_requests.load()}
                }
            );
        } else {
            stats_.routing_errors++;
        }
        
        return selected;
    }
private:
    RoutingStrategy strategy_;
    std::atomic<bool> running_;
    
    std::vector<std::shared_ptr<BackendNode>> real_backends_;
    std::vector<std::shared_ptr<BackendNode>> honeypot_backends_;
    std::mutex backends_mutex_;
    
    std::atomic<size_t> round_robin_index_{0};
    std::thread health_check_thread_;
    
    SubscriptionId health_check_sub_;
    SubscriptionId classification_sub_;
    SubscriptionId response_sub_;
    
    // Статистика
    LoadBalancerStats stats_;
    PerformanceStats performance_;

    // Вспомогательные методы для статистики
    static std::string strategy_to_string(RoutingStrategy strategy) {
        switch (strategy) {
            case RoutingStrategy::ROUND_ROBIN: return "Round Robin";
            case RoutingStrategy::LEAST_CONNECTIONS: return "Least Connections";
            case RoutingStrategy::IP_HASH: return "IP Hash";
            case RoutingStrategy::WEIGHTED: return "Weighted";
            default: return "Unknown";
        }
    }

    void handle_response_metrics(const Event& event) {
        if (event.data.contains("server_id") && event.data.contains("response_time_ms") && event.data.contains("success")) {
            std::string server_id = event.data["server_id"].get_or(std::string(""));
            bool success = event.data["success"].get_or(false);
            auto response_time = std::chrono::milliseconds(event.data["response_time_ms"].get_or(0));
            
            if (success) {
                mark_request_success(server_id, response_time);
            } else {
                mark_request_failure(server_id);
            }
        }
    }

    void handle_health_update(const Event& event) {
        // Implement health update logic
    }

    void handle_classification(const Event& event) {
        // Implement classification logic
    }

    void mark_request_success(const std::string& server_id, std::chrono::milliseconds response_time) {
        // Implement success tracking
    }

    void mark_request_failure(const std::string& server_id) {
        // Implement failure tracking
    }
};