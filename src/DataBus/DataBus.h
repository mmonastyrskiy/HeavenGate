#ifndef DATABUS_HPP
#define DATABUS_HPP

#pragma GCC diagnostic ignored "-Wc++17-extensions"
#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <future>
#include <algorithm>
#include <string>
#include <stdexcept>

#include "../../thirdparty/json.hpp"

#include "BusEvent.h"
#include "subscriptionID.h"
#include "DataBusMetrics.h"

class DataBus {
public:
    static constexpr size_t MAX_QUEUE_SIZE = 10000; // TODO: Move to config
    
    static DataBus& instance();

    // Публикация событий
    void publish(BusEventType type, const std::string& source, const nlohmann::json& data);

    // Подписка на события
    SubscriptionId subscribe(BusEventType type, EventCallback callback);

    // Отписка от событий
    void unsubscribe(SubscriptionId id);

    // Синхронный запрос-ответ
    nlohmann::json request(BusEventType type, const nlohmann::json& data, 
                          std::chrono::milliseconds timeout = std::chrono::seconds(1));

    // Запуск шины
    void start();

    // Остановка шины
    void stop();

    // Получение метрик
    DataBusMetricsSnapshot get_metrics() const;

private:
    DataBus() = default;
    ~DataBus();

    DataBus(const DataBus&) = delete;
    DataBus& operator=(const DataBus&) = delete;

    struct Subscriber {
        SubscriptionId id;
        EventCallback callback;
    };

    // Очередь событий
    std::queue<Event> events_queue_;
    mutable std::mutex events_mutex_;
    std::condition_variable events_cv_;
    
    // Подписки
    std::unordered_map<BusEventType, std::vector<Subscriber>> subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    
    // Механизм запрос-ответ
    std::unordered_map<std::string, std::promise<nlohmann::json>> pending_requests_;
    mutable std::mutex requests_mutex_;
    
    // Управление потоками
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    
    // Генерация ID
    std::atomic<SubscriptionId> next_subscription_id_{1};
    std::atomic<uint64_t> next_event_id_{1};
    std::atomic<uint64_t> next_correlation_id_{1};
    
    // Метрики
    DataBusMetricsInternal metrics_;

    void process_events();
    void handle_event(const Event& event);
    std::future<nlohmann::json> setup_response_waiter(const std::string& correlation_id);
    void cleanup();
    std::string generate_event_id();
    std::string generate_correlation_id();
};

#endif // DATABUS_HPP