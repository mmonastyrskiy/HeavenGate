#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <random>
#include <memory>
#include "LoadBalancer.h"
#include "LoadBalancerMetrics.h"
#include "../DataBus/DataBus.h"
#include "../DataBus/BusEvent.h"
#include "../DataBus/subscriptionID.h"
#include "../../thirdparty/json.hpp"

// Forward declarations or include proper headers

using EventCallback = std::function<void(const Event&)>;
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
health_check_sub_ = DataBus::instance().subscribe(
    BusEventType::SERVICE_HEALTH_UPDATE,
    [this](const Event &event) {
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
            if (backend->is_healthy.load()) {
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
            selected->current_clients++;
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
                BusEventType::REQUEST_ROUTED,
                (std::string) "load_balancer",
                {
                    {"client_ip", client_ip},
                    {"server_id", selected->id},
                    {"is_malicious", is_malicious},
                    {"strategy", static_cast<int>(strategy_)},
                    {"current_connections", selected->current_clients.load()},
                    {"routing_time_ns", routing_time_ns},
                    {"total_requests", selected->total_requests.load()}
                }
            );
        } else {
            stats_.routing_errors++;
        }
        
        return selected;
    }

    void release_backend(const std::string& server_id) {
        std::lock_guard<std::mutex> lock(backends_mutex_);
        
        auto find_server = [&](const std::vector<std::shared_ptr<BackendNode>>& backends) {
            for (const auto& server : backends) {
                if (server->id == server_id) {
                    server->current_clients--;
                    return true;
                }
            }
            return false;
        };
        
        if (!find_server(real_backends_)) {
            find_server(honeypot_backends_);
        }
    }



 void start_health_checks(std::chrono::seconds interval = std::chrono::seconds(30)) {
        if (running_.exchange(true)) return;
        
        health_check_thread_ = std::thread([this, interval]() {
            while (running_.load()) {
                perform_health_checks();
                std::this_thread::sleep_for(interval);
            }
        });
    }

      void stop() {
        if (!running_.exchange(false)) return;
        
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
    }
    void set_routing_strategy(RoutingStrategy strategy) {
        strategy_ = strategy;
    }

LoadBalancerStats get_stats() const {
        std::lock_guard<std::mutex> lock(backends_mutex_);
        
        LoadBalancerStats stats{};
        stats.total_real_backends = real_backends_.size();
        stats.total_honeypot_backends = honeypot_backends_.size();
        stats.total_connections = 0;
        
        for (const auto& backend : real_backends_) {
            if (backend->is_healthy.load()) stats.healthy_real_backends++;
            stats.total_connections += backend->current_clients.load();
        }
        
        for (const auto& backend : honeypot_backends_) {
            if (backend->is_healthy.load()) stats.healthy_honeypot_backends++;
            stats.total_connections += backend->current_clients.load();
        }
        
        return stats;
    }

private:
    RoutingStrategy strategy_;
    std::atomic<bool> running_;
    
    std::vector<std::shared_ptr<BackendNode>> real_backends_;
    std::vector<std::shared_ptr<BackendNode>> honeypot_backends_;
    mutable std::mutex backends_mutex_;
    
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
            std::string server_id = event.data["server_id"];
            bool success = event.data["success"];
            auto response_time = std::chrono::milliseconds(event.data["response_time_ms"]);
            
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
 std::shared_ptr<BackendNode> round_robin_selection(
        const std::vector<std::shared_ptr<BackendNode>>& backends) {
        
        size_t index = round_robin_index_++ % backends.size();
        return backends[index];
    }
std::shared_ptr<BackendNode> least_connections_selection(
        const std::vector<std::shared_ptr<BackendNode>>& backends) {
        
        return *std::min_element(backends.begin(), backends.end(),
            [](const auto& a, const auto& b) {
                return a->current_clients.load() < b->current_clients.load();
            });
    }

    std::shared_ptr<BackendNode> ip_hash_selection(
        const std::vector<std::shared_ptr<BackendNode>>& backends,
        const std::string& client_ip) {
        
        if (client_ip.empty()) {
            return round_robin_selection(backends);
        }
        
        std::hash<std::string> hasher;
        size_t hash = hasher(client_ip);
        size_t index = hash % backends.size();
        
        return backends[index];
    }


    std::shared_ptr<BackendNode> weighted_selection(
        const std::vector<std::shared_ptr<BackendNode>>& backends) {
        
        int total_weight = 0;
        for (const auto& backend : backends) {
            total_weight += backend->weight;
        }
        
        if (total_weight == 0) {
            return round_robin_selection(backends);
        }
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, total_weight);
        
        int random_weight = dis(gen);
        int current_weight = 0;
        
        for (const auto& backend : backends) {
            current_weight += backend->weight;
            if (random_weight <= current_weight) {
                return backend;
            }
        }
        
        return backends.back();
    }

    void perform_health_checks() {
        std::lock_guard<std::mutex> lock(backends_mutex_);
        
        auto check_backends = [this](std::vector<std::shared_ptr<BackendNode>>& backends) {
            for (auto& backend : backends) {
                bool was_healthy = backend->is_healthy.load();
                bool is_healthy = check_server_health(backend);
                
                backend->is_healthy = is_healthy;
                backend->last_health_check = std::chrono::steady_clock::now();
                
                if (was_healthy != is_healthy) {
                    DataBus::instance().publish(
                        BusEventType::SERVICE_HEALTH_UPDATE,
                        (std::string)"load_balancer",
                        {
                            {"server_id", backend->id},
                            {"host", backend->host},
                            {"port", backend->port},
                            {"is_honeypot", backend->is_honeypot},
                            {"healthy", is_healthy},
                            {"current_connections", backend->current_clients.load()}
                        }
                    );
                }
            }
        };
        
        check_backends(real_backends_);
        check_backends(honeypot_backends_);
    }

     bool check_server_health(const std::shared_ptr<BackendNode>& server) {
        // Простая реализация health check - можно расширить
        try {
            // Здесь может быть HTTP запрос, TCP соединение и т.д.
            // Для прототипа просто возвращаем true
            return true;
        } catch (...) {
            return false;
        }
    }

    void handle_health_update(const Event& event) {
        // Обработка внешних health updates (например, от Service Manager)
        if (event.data.contains("server_id") && event.data.contains("healthy")) {
            std::string server_id = event.data["server_id"];
            bool healthy = event.data["healthy"];
            
            std::lock_guard<std::mutex> lock(backends_mutex_);
            
            auto update_health = [&](std::vector<std::shared_ptr<BackendNode>>& backends) {
                for (auto& backend : backends) {
                    if (backend->id == server_id) {
                        backend->is_healthy = healthy;
                        return true;
                    }
                }
                return false;
            };
             if (!update_health(real_backends_)) {
                update_health(honeypot_backends_);
            }
        }
    }
    void handle_classification(const Event& event) {
        // Сохранение классификации для последующего использования
        // при выборе бэкенда
        if (event.data.contains("client_ip") && event.data.contains("classification")) {
            // Можно кэшировать решения классификатора
            // для оптимизации последующих запросов
        }
    }


};