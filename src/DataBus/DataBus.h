
/*
 * Filename: d:\HeavenGate\src\DataBus\DataBus.h
 * Path: d:\HeavenGate\src\DataBus
 * Created Date: Saturday, November 8th 2025, 8:50:32 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */


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

#include "../../thirdparty/json.hpp"

#include "BusEvent.h"
#include "subscriptionID.h"
#include "DataBusMetrics.h"
#include "../common/Confparcer.h"

class DataBus {
public:
        const size_t MAX_QUEUE_SIZE = []() {
        return Confparcer::SETTING<size_t>("MAX_BUS_QUEUE_SIZE", 100000);
    }();

        static inline const size_t TIMEOUT = [](){
        return Confparcer::SETTING<size_t>("BUS_REQUST_TIMEOUT", 1);
    }();

    static DataBus& instance();
    void publish(BusEventType type, const std::string& source, const nlohmann::json& data);
    SubscriptionId subscribe(BusEventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);
    nlohmann::json request(BusEventType type, const nlohmann::json& data,
                           std::chrono::milliseconds timeout = std::chrono::seconds(TIMEOUT));
    void start();
    void stop();
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


    std::queue<Event> events_queue_;
    mutable std::mutex events_mutex_;
    std::condition_variable events_cv_;

    std::unordered_map<BusEventType, std::vector<Subscriber>> subscriptions_;
    mutable std::mutex subscriptions_mutex_;

    std::unordered_map<std::string, std::promise<nlohmann::json>> pending_requests_;
    mutable std::mutex requests_mutex_;

    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    std::atomic<SubscriptionId> next_subscription_id_{1};
    std::atomic<uint64_t> next_event_id_{1};
    std::atomic<uint64_t> next_correlation_id_{1};

    DataBusMetricsInternal metrics_;

    void process_events();
    void handle_event(const Event& event);
    std::future<nlohmann::json> setup_response_waiter(const std::string& correlation_id);
    void cleanup();
    std::string generate_event_id();
    std::string generate_correlation_id();
};
