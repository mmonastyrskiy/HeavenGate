#include "LoadBalancer.h"
#include <algorithm>
#include <iostream>
#include "../API/dashboardAPI.h"

BackendNode::BackendNode(const std::string& id, const std::string& host, int port,
                         bool is_honeypot, float weight)
: id(id), host(host), port(port), is_honeypot(is_honeypot), weight(weight)
{}

LoadBalancer::LoadBalancer(RoutingStrategy strategy)
: strategy_(strategy), running_(false) {

    stats_.start_time = std::chrono::steady_clock::now();

    health_check_sub_ = DataBus::instance().subscribe(
        BusEventType::SERVICE_HEALTH_UPDATE,
        [this](const Event& event) {
            this->handle_health_update(event);
        }
    );

    classification_sub_ = DataBus::instance().subscribe(
        BusEventType::REQUEST_CLASSIFIED,
        [this](const Event& event) {
            this->handle_classification(event);
        }
    );

    response_sub_ = DataBus::instance().subscribe(
        BusEventType::REQUEST_PROCESSED,
        [this](const Event& event) {
            this->handle_response_metrics(event);
        }
    );
}

LoadBalancer::~LoadBalancer() {
    stop();
    DataBus::instance().unsubscribe(health_check_sub_);
    DataBus::instance().unsubscribe(classification_sub_);
    DataBus::instance().unsubscribe(response_sub_);
}

void LoadBalancer::add_backend(std::shared_ptr<BackendNode> server_ptr) {
    std::lock_guard<std::mutex> lock(backends_mutex_);

    if (server_ptr->is_honeypot) {
        honeypot_backends_.push_back(server_ptr);
    } else {
        real_backends_.push_back(server_ptr);
    }

    DataBus::instance().publish(
        BusEventType::SERVICE_REGISTERED,
        "load_balancer",
        {
            {"server_id", server_ptr->id},
            {"host", server_ptr->host},
            {"port", std::to_string(server_ptr->port)},
                                {"is_honeypot", server_ptr->is_honeypot ? "true" : "false"},
                                {"weight", std::to_string(server_ptr->weight)}
        }
    );
}

std::shared_ptr<BackendNode> LoadBalancer::select_backend(bool is_malicious, const std::string& client_ip) {
    auto start_time = std::chrono::steady_clock::now();

    auto& backends = is_malicious ? honeypot_backends_ : real_backends_;

    if (backends.empty()) {
        stats_.routing_errors++;
        performance_.backend_selection_failures++;
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(backends_mutex_);

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
            "load_balancer",
            nlohmann::json{
                {"client_ip", client_ip},
                {"server_id", selected->id},
                {"is_malicious", is_malicious},
                {"strategy", static_cast<int>(strategy_)},
                                    {"current_connections", selected->current_clients.load()},
                                    {"routing_time_ns", routing_time_ns},
                                    {"total_requests", selected->total_requests.load()}
            }
        );
        int err = 0;
        DashboardAPI::the().callUserRegistered(client_ip,selected->id,is_malicious,&err);

    } else {
        stats_.routing_errors++;
    }

    return selected;
}

void LoadBalancer::release_backend(const std::string& server_id) {
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

void LoadBalancer::start_health_checks(std::chrono::seconds interval) {
    if (running_.exchange(true)) return;

    health_check_thread_ = std::thread([this, interval]() {
        while (running_.load()) {
            perform_health_checks();
            std::this_thread::sleep_for(interval);
        }
    });
}

void LoadBalancer::stop() {
    if (!running_.exchange(false)) return;

    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
}

void LoadBalancer::set_routing_strategy(RoutingStrategy strategy) {
    strategy_ = strategy;
}

LoadBalancerStats LoadBalancer::get_stats() const {
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

std::shared_ptr<BackendNode> LoadBalancer::round_robin_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
    size_t index = round_robin_index_++ % backends.size();
    return backends[index];
}

std::shared_ptr<BackendNode> LoadBalancer::least_connections_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
    return *std::min_element(backends.begin(), backends.end(),
                             [](const auto& a, const auto& b) {
                                 return a->current_clients.load() < b->current_clients.load();
                             });
}

std::shared_ptr<BackendNode> LoadBalancer::ip_hash_selection(const std::vector<std::shared_ptr<BackendNode>>& backends, const std::string& client_ip) {
    if (client_ip.empty()) {
        return round_robin_selection(backends);
    }

    std::hash<std::string> hasher;
    size_t hash = hasher(client_ip);
    size_t index = hash % backends.size();

    return backends[index];
}

std::shared_ptr<BackendNode> LoadBalancer::weighted_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
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

void LoadBalancer::perform_health_checks() {
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
                    "load_balancer",
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

bool LoadBalancer::check_server_health(const std::shared_ptr<BackendNode>& server) {
    try {
        return true;
    } catch (...) {
        return false;
    }
}

void LoadBalancer::handle_health_update(const Event& event) {
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

void LoadBalancer::handle_classification(const Event& event) {
    if (event.data.contains("client_ip") && event.data.contains("classification")) {
    }
}

void LoadBalancer::handle_response_metrics(const Event& event) {
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

void LoadBalancer::mark_request_success(const std::string& server_id, std::chrono::milliseconds response_time) {
}

void LoadBalancer::mark_request_failure(const std::string& server_id) {
}

std::string LoadBalancer::strategy_to_string(RoutingStrategy strategy) {
    switch (strategy) {
        case RoutingStrategy::ROUND_ROBIN: return "Round Robin";
        case RoutingStrategy::LEAST_CONNECTIONS: return "Least Connections";
        case RoutingStrategy::IP_HASH: return "IP Hash";
        case RoutingStrategy::WEIGHTED: return "Weighted";
        default: return "Unknown";
    }
}
