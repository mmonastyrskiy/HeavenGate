/*
 * Filename: d:\HeavenGate\src\LoadBalancer\LoadBalancer.cpp
 * Path: d:\HeavenGate\src\LoadBalancer
 * Created Date: Saturday, November 9th 2025
 * Author: mmonastyrskiy
 */

#include "LoadBalancer.h"
#include "../common/logger.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <functional>
#include "../API/dashboardAPI.h"

// ClientConnection implementation
ClientConnection::ClientConnection(asio::io_context& io_context, const std::string& ip)
    : client_ip(ip), socket(io_context) {
    client_id = ip + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

void ClientConnection::start() {
    // Client handling will be implemented in LoadBalancer
}

void ClientConnection::close() {
    active = false;
    asio::error_code ec;
    socket.close(ec);
    if (backend_socket) {
        backend_socket->close(ec);
    }
}

// BackendNode implementation
BackendNode::BackendNode(const std::string& id, const std::string& host, int port,
                         bool is_honeypot, float weight)
    : id(id), host(host), port(port), is_honeypot(is_honeypot), weight(weight),
      last_request_time(std::chrono::steady_clock::now()),
      last_health_check(std::chrono::steady_clock::now()) {}

// LoadBalancer implementation
LoadBalancer::LoadBalancer(RoutingStrategy strategy)
    : strategy_(strategy), running_(false), acceptor_(io_context_) {

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

void LoadBalancer::start(int port) {
    if (running_.exchange(true)) return;

    try {
        asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        LOG_INFO("LoadBalancer started on port " + std::to_string(port));

        server_thread_ = std::thread([this]() {
            start_accept();
            io_context_.run();
        });

        start_health_checks();

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start LoadBalancer: " + std::string(e.what()));
        running_ = false;
    }
}

void LoadBalancer::stop() {
    if (!running_.exchange(false)) return;

    io_context_.stop();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }

    LOG_INFO("LoadBalancer stopped");
}

void LoadBalancer::start_accept() {
    auto client = std::make_shared<ClientConnection>(io_context_, "");
    
    acceptor_.async_accept(client->socket,
        [this, client](const asio::error_code& error) {
            handle_accept(client, error);
        });
}

void LoadBalancer::handle_accept(ClientConnection::Ptr client, const asio::error_code& error) {
    if (!error) {
        // Get client IP
        asio::error_code ec;
        auto remote_ep = client->socket.remote_endpoint(ec);
        if (!ec) {
            client->client_ip = remote_ep.address().to_string();
        }

        // Publish new client connection event
        DataBus::instance().publish(
            BusEventType::NEW_CLIENT_CONNECTION,
            "load_balancer",
            nlohmann::json{
                {"client_ip", client->client_ip},
                {"client_id", client->client_id},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            }
        );

        LOG_INFO("New client connected: " + client->client_ip);

        // Start handling client requests
        handle_client_request(client);
        
        // Continue accepting new connections
        start_accept();
    } else {
        LOG_ERROR("Accept error: " + error.message());
        if (running_.load()) {
            start_accept();
        }
    }
}

void LoadBalancer::handle_client_request(ClientConnection::Ptr client) {
    // Check if client already has assigned backend
    auto assigned_backend = get_assigned_backend(client->client_ip);
    
    if (!assigned_backend) {
        // For initial request, send to classifier first
        read_from_client(client);
    } else {
        // Client already classified, proxy directly to assigned backend
        proxy_to_backend(client, assigned_backend);
    }
}

void LoadBalancer::read_from_client(ClientConnection::Ptr client) {
    auto buffer = std::make_shared<std::vector<char>>(8192);
    
    client->socket.async_read_some(asio::buffer(*buffer),
        [this, client, buffer](const asio::error_code& error, size_t bytes_read) {
            if (!error && bytes_read > 0) {
                // Publish request to classifier
                std::string request_data(buffer->data(), bytes_read);
                
                DataBus::instance().publish(
                    BusEventType::REQUEST_FOR_CLASSIFICATION,
                    "load_balancer",
                    nlohmann::json{
                        {"client_ip", client->client_ip},
                        {"client_id", client->client_id},
                        {"request_data", request_data},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()},
                        {"client_ptr", reinterpret_cast<uintptr_t>(client.get())}
                    }
                );
                
                LOG_DEBUG("Request sent to classifier from client: " + client->client_ip);
                
                // Store the received data temporarily
                // We'll use it when we get classification result
                // For now, we wait for classification
                
            } else if (error != asio::error::operation_aborted) {
                LOG_WARN("Read from client failed: " + error.message());
                client->close();
            }
        });
}

void LoadBalancer::proxy_to_backend(ClientConnection::Ptr client, std::shared_ptr<BackendNode> backend) {
    try {
        if (!client->backend_socket) {
            client->backend_socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
            
            asio::ip::tcp::endpoint backend_ep(
                asio::ip::make_address(backend->host), backend->port);
            
            client->backend_socket->async_connect(backend_ep,
                [this, client, backend](const asio::error_code& error) {
                    if (!error) {
                        // Start bidirectional proxying
                        read_from_client(client);
                        read_from_backend(client);
                    } else {
                        LOG_ERROR("Backend connection failed: " + error.message());
                        client->close();
                        release_backend(backend->id);
                    }
                });
        } else {
            // Continue normal proxying
            read_from_client(client);
            read_from_backend(client);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Proxy error: " + std::string(e.what()));
        client->close();
        release_backend(backend->id);
    }
}

void LoadBalancer::read_from_backend(ClientConnection::Ptr client) {
    auto buffer = std::make_shared<std::vector<char>>(8192);
    
    if (client->backend_socket && client->backend_socket->is_open()) {
        client->backend_socket->async_read_some(asio::buffer(*buffer),
            [this, client, buffer](const asio::error_code& error, size_t bytes_read) {
                if (!error && bytes_read > 0) {
                    // Forward backend response to client
                    asio::async_write(client->socket, asio::buffer(buffer->data(), bytes_read),
                        [this, client](const asio::error_code& error, size_t /*bytes_written*/) {
                            if (!error) {
                                read_from_backend(client);
                            } else {
                                LOG_WARN("Write to client failed: " + error.message());
                                client->close();
                            }
                        });
                } else if (error != asio::error::operation_aborted) {
                    LOG_WARN("Read from backend failed: " + error.message());
                    client->close();
                }
            });
    }
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
    
    LOG_INFO("New host registered IP: " + server_ptr->host + ":" + 
             std::to_string(server_ptr->port) + " Is honeypot: " + 
             (server_ptr->is_honeypot ? "true" : "false"));

    DashboardAPI::the().callAgentChange(real_backends_.size(), honeypot_backends_.size());
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
        DashboardAPI::the().callUserRegistered(client_ip, selected->id, is_malicious, &err);

    } else {
        stats_.routing_errors++;
    }

    return selected;
}

std::shared_ptr<BackendNode> LoadBalancer::get_assigned_backend(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    auto it = client_backend_mapping_.find(client_ip);
    return (it != client_backend_mapping_.end()) ? it->second : nullptr;
}

void LoadBalancer::assign_backend_to_client(const std::string& client_ip, std::shared_ptr<BackendNode> backend) {
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    client_backend_mapping_[client_ip] = backend;
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

void LoadBalancer::handle_classification(const Event& event) {
    if (event.data.contains("client_ip") && event.data.contains("classification")) {
        std::string client_ip = event.data["client_ip"];
        bool is_malicious = event.data["classification"] == "malicious";
        
        // Get client pointer if available
        uintptr_t client_ptr_val = 0;
        if (event.data.contains("client_ptr")) {
            client_ptr_val = event.data["client_ptr"];
        }

        LOG_INFO("Client classified: " + client_ip + " as " + 
                 (is_malicious ? "malicious" : "benign"));

        // Select backend based on classification
        auto backend = select_backend(is_malicious, client_ip);
        if (backend) {
            assign_backend_to_client(client_ip, backend);
            
            // If we have client pointer, we can resume request processing
            if (client_ptr_val != 0) {
                // Note: In real implementation, you'd need to map client_ptr_val back to ClientConnection
                // This might require maintaining a registry of active connections
            }
        } else {
            LOG_ERROR("No available backend for client: " + client_ip);
        }
    }
}

// Selection strategy implementations
std::shared_ptr<BackendNode> LoadBalancer::round_robin_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
    if (backends.empty()) return nullptr;
    size_t index = round_robin_index_++ % backends.size();
    return backends[index];
}

std::shared_ptr<BackendNode> LoadBalancer::least_connections_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
    if (backends.empty()) return nullptr;
    return *std::min_element(backends.begin(), backends.end(),
                             [](const auto& a, const auto& b) {
                                 return a->current_clients.load() < b->current_clients.load();
                             });
}

std::shared_ptr<BackendNode> LoadBalancer::ip_hash_selection(const std::vector<std::shared_ptr<BackendNode>>& backends, const std::string& client_ip) {
    if (backends.empty()) return nullptr;
    if (client_ip.empty()) {
        return round_robin_selection(backends);
    }

    std::hash<std::string> hasher;
    size_t hash = hasher(client_ip);
    size_t index = hash % backends.size();

    return backends[index];
}

std::shared_ptr<BackendNode> LoadBalancer::weighted_selection(const std::vector<std::shared_ptr<BackendNode>>& backends) {
    if (backends.empty()) return nullptr;

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

// Health checking implementation
void LoadBalancer::start_health_checks(std::chrono::seconds interval) {
    if (running_.exchange(true)) return;

    health_check_thread_ = std::thread([this, interval]() {
        while (running_.load()) {
            perform_health_checks();
            std::this_thread::sleep_for(interval);
        }
    });
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
                        {"port", std::to_string(backend->port)},
                        {"is_honeypot", backend->is_honeypot},
                        {"healthy", is_healthy},
                        {"current_connections", backend->current_clients.load()}
                    }
                );
                
                LOG_INFO("Backend " + backend->id + " health changed: " + 
                         (is_healthy ? "healthy" : "unhealthy"));
            }
        }
    };

    check_backends(real_backends_);
    check_backends(honeypot_backends_);
}

bool LoadBalancer::check_server_health(const std::shared_ptr<BackendNode>& server) {
    try {
        // Simple TCP connection check
        asio::io_context io_context;
        asio::ip::tcp::socket socket(io_context);
        asio::ip::tcp::endpoint endpoint(
            asio::ip::make_address(server->host), 
            server->port
        );
        
        socket.connect(endpoint, std::chrono::seconds(2));
        bool is_healthy = socket.is_open();
        socket.close();
        
        return is_healthy;
    } catch (const std::exception& e) {
        LOG_WARN("Health check failed for " + server->id + ": " + e.what());
        return false;
    }
}

// Event handlers
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
    std::lock_guard<std::mutex> lock(backends_mutex_);
    
    auto update_server = [&](std::vector<std::shared_ptr<BackendNode>>& backends) {
        for (auto& backend : backends) {
            if (backend->id == server_id) {
                // Update server metrics for successful request
                backend->last_request_time = std::chrono::steady_clock::now();
                return true;
            }
        }
        return false;
    };
    
    update_server(real_backends_) || update_server(honeypot_backends_);
}

void LoadBalancer::mark_request_failure(const std::string& server_id) {
    std::lock_guard<std::mutex> lock(backends_mutex_);
    
    auto update_server = [&](std::vector<std::shared_ptr<BackendNode>>& backends) {
        for (auto& backend : backends) {
            if (backend->id == server_id) {
                // Update server metrics for failed request
                // Could implement circuit breaker pattern here
                return true;
            }
        }
        return false;
    };
    
    update_server(real_backends_) || update_server(honeypot_backends_);
}

// Statistics and utility methods
LoadBalancerStats LoadBalancer::get_stats() const {
    std::lock_guard<std::mutex> lock(backends_mutex_);

    LoadBalancerStats stats = stats_;
    stats.total_real_backends = real_backends_.size();
    stats.total_honeypot_backends = honeypot_backends_.size();
    stats.total_connections = 0;
    stats.healthy_real_backends = 0;
    stats.healthy_honeypot_backends = 0;

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

PerformanceMetrics LoadBalancer::get_performance_metrics() const {
    return performance_;
}

void LoadBalancer::set_routing_strategy(RoutingStrategy strategy) {
    strategy_ = strategy;
    LOG_INFO("Routing strategy changed to: " + strategy_to_string(strategy));
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