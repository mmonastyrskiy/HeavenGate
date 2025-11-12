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
        LOG_FATAL("Failed to start LoadBalancer: " + std::string(e.what()));
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

// Остальные методы остаются без изменений...
// [round_robin_selection, least_connections_selection, ip_hash_selection, weighted_selection,
//  start_health_checks, perform_health_checks, check_server_health, handle_health_update,
//  handle_response_metrics, mark_request_success, mark_request_failure, get_stats, strategy_to_string]