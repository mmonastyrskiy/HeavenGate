/*
 * Filename: d:\HeavenGate\src\main.cpp
 * Path: d:\HeavenGate\src
 * Created Date: Sunday, November 9th 2025, 12:15:21 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include "LoadBalancer/LoadBalancer.h"
#include "DataBus/DataBus.h"
#include "AppManager/AppManager.h"
#include "common/logger.h"

std::atomic<bool> running{true};

void signalHandler(int sig) {
    if (sig == SIGINT) {
        running = false;
        std::cout << "\n🛑 Received SIGINT, shutting down..." << std::endl;
    }
}

void printStats(const LoadBalancer& balancer) {
    auto stats = balancer.get_stats();
    const auto& metrics = balancer.get_performance_metrics(); // const reference
    
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - stats.start_time);
    
    std::cout << "\n📊 === Load Balancer Statistics ===" << std::endl;
    std::cout << "⏱️  Uptime: " << uptime.count() << "s" << std::endl;
    std::cout << "📨 Total Requests: " << stats.total_requests_processed << std::endl;
    std::cout << "✅ Real Backends: " << stats.requests_routed_to_real << std::endl;
    std::cout << "🚨 Honeypot Backends: " << stats.requests_routed_to_honeypot << std::endl;
    std::cout << "❌ Routing Errors: " << stats.routing_errors << std::endl;
    std::cout << "🖥️  Active Real Servers: " << stats.healthy_real_backends << "/" << stats.total_real_backends << std::endl;
    std::cout << "🍯 Active Honeypots: " << stats.healthy_honeypot_backends << "/" << stats.total_honeypot_backends << std::endl;
    std::cout << "🔗 Total Connections: " << stats.total_connections << std::endl;
    
    if (metrics.total_routing_operations > 0) {
        double avg_routing_time = static_cast<double>(metrics.total_routing_time_ns) / 
                                 metrics.total_routing_operations / 1000.0; // convert to microseconds
        std::cout << "⚡ Avg Routing Time: " << avg_routing_time << " μs" << std::endl;
    }
    std::cout << "================================\n" << std::endl;
}

int main() {
    AppManager manager;
    manager.start_all();

    std::cout << "🚀 Starting HeavenGate Load Balancer" << std::endl;
    std::cout << "📍 Listening on port 80" << std::endl;
    
    std::signal(SIGINT, signalHandler);
    
    try {
        // Создаем балансировщик с стратегией IP_HASH для sticky sessions
        LoadBalancer balancer(RoutingStrategy::IP_HASH);

        // Добавляем реальные бэкенды
        balancer.add_backend(std::make_shared<BackendNode>(
            "real-server-1", "127.0.0.1", 8080, false, 1.0f));
        balancer.add_backend(std::make_shared<BackendNode>(
            "real-server-2", "127.0.0.1", 8081, false, 1.0f));
        balancer.add_backend(std::make_shared<BackendNode>(
            "real-server-3", "127.0.0.1", 8082, false, 1.5f)); // Более мощный сервер

        // Добавляем honeypot серверы
        balancer.add_backend(std::make_shared<BackendNode>(
            "honeypot-1", "127.0.0.1", 9090, true, 1.0f));
        balancer.add_backend(std::make_shared<BackendNode>(
            "honeypot-2", "127.0.0.1", 9091, true, 1.0f));

        std::cout << "✅ Backends registered:" << std::endl;
        std::cout << "   - 3 real servers (8080, 8081, 8082)" << std::endl;
        std::cout << "   - 2 honeypot servers (9090, 9091)" << std::endl;

        // Запускаем балансировщик на порту 80
        balancer.start(80);

        std::cout << "\n✅ Load Balancer started successfully!" << std::endl;
        std::cout << "💡 Press Ctrl+C to stop the server\n" << std::endl;

        // Статистика каждые 30 секунд
        auto last_stats_time = std::chrono::steady_clock::now();
        const auto stats_interval = std::chrono::seconds(30);

        // Основной цикл
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Периодический вывод статистики
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time >= stats_interval) {
                printStats(balancer);
                last_stats_time = now;
            }
        }

        // Остановка балансировщика
        std::cout << "🛑 Stopping Load Balancer..." << std::endl;
        balancer.stop();
        
        // Финальная статистика
        std::cout << "\n📈 === Final Statistics ===" << std::endl;
        printStats(balancer);

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal Error: " << e.what() << std::endl;
        LOG_ERROR("Main application error: " + std::string(e.what()));
        manager.stop_all();
        return 1;
    }

    std::cout << "✅ HeavenGate stopped gracefully" << std::endl;
    manager.stop_all();
    return 0;
}