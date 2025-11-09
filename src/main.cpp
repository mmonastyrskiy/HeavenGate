/*
 * Filename: d:\HeavenGate\src\main.cpp
 * Path: d:\HeavenGate\src
 * Created Date: Sunday, November 9th 2025, 12:15:21 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */


#include <iostream>
#include "LoadBalancer/LoadBalancer.h"
#include "DataBus/DataBus.h"
#include "../include/colorText.h"

std::atomic<bool> running{true};

int main() {
     color::print::red() << "Text";

    std::cout << "Starting HeavenGate Load Balancer" << std::endl;
    try {
        // Создаем балансировщик
        LoadBalancer balancer(RoutingStrategy::ROUND_ROBIN);

        // Добавляем тестовые бэкенды
        balancer.add_backend(std::make_shared<BackendNode>(
            "real-server-1", "127.0.0.1", 8080, false, 1.0f));

        balancer.add_backend(std::make_shared<BackendNode>(
            "real-server-2", "127.0.0.1", 8081, false, 1.0f));

        balancer.add_backend(std::make_shared<BackendNode>(
            "honeypot-1", "127.0.0.1", 8082, true, 1.0f));
        // Запускаем health checks
        balancer.start_health_checks(std::chrono::seconds(10));

        std::cout << "✅ Load Balancer initialized!" << std::endl;

        // Имитируем запросы
        int request_count = 0;
        while (running && request_count < 10) {
            bool is_malicious = (request_count % 5 == 0);
            std::string client_ip = "192.168.1." + std::to_string(100 + (request_count % 5));

            auto backend = balancer.select_backend(is_malicious, client_ip);
            if (backend) {
                std::cout << "📨 Request " << request_count
                << " from " << client_ip
                << " -> " << (backend->is_honeypot ? "🚨 HONEYPOT" : "✅ REAL")
                << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                balancer.release_backend(backend->id);
            }

            request_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        balancer.stop();

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "✅ HeavenGate stopped" << std::endl;
    return 0;
}
