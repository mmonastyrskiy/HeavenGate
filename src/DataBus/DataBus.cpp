/*
 * Filename: d:\HeavenGate\src\DataBus\DataBus.cpp
 * Path: d:\HeavenGate\src\DataBus
 * Created Date: Saturday, November 8th 2025, 8:50:32 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#include "DataBus.h"
#include "../common/logger.h"

DataBus& DataBus::instance() {
    static DataBus instance;
    return instance;
}

void DataBus::publish(BusEventType type, const std::string& source, const nlohmann::json& data) {
    Event event;
    event.type = type;
    event.source = source;
    event.data = data;
    event.timestamp = std::chrono::system_clock::now();
    event.id = generate_event_id();

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        events_queue_.push(event);
        logger::Logger::info("Event pushed to the bus by " + source);
        metrics_.queue_size = events_queue_.size();

        if(metrics_.queue_size >= MAX_QUEUE_SIZE){
            events_queue_.pop();
            metrics_.queue_overflow++;
            logger::Logger::warn("Queue overflow");
        }
    }
    events_cv_.notify_one();

    metrics_.events_published++;
}

SubscriptionId DataBus::subscribe(BusEventType type, EventCallback callback) {
    SubscriptionId id = next_subscription_id_++;

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_[type].push_back({id, callback});
    }

    return id;
}

void DataBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    for (auto& [type, subscribers] : subscriptions_) {
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                           [id](const auto& sub) { return sub.id == id; }),
                          subscribers.end()
        );
    }
}

nlohmann::json DataBus::request(BusEventType type, const nlohmann::json& data,
                                std::chrono::milliseconds timeout) {
    std::string correlation_id = generate_correlation_id();
    auto response_future = setup_response_waiter(correlation_id);

    publish(type, "requestor", {
        {"data", data},
        {"correlation_id", correlation_id},
        {"is_request", true}
    });
    

    if (response_future.wait_for(timeout) == std::future_status::ready) {
        return response_future.get();
    }

    {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_requests_.erase(correlation_id);
    }

    throw std::runtime_error("Request timeout");
                                }

                                void DataBus::start() {
                                    if (running_.exchange(true)) return;

                                    worker_thread_ = std::thread(&DataBus::process_events, this);
                                    logger::Logger::info("Bus worker started");
                                }

                                void DataBus::stop() {
                                    if (!running_.exchange(false)) return;

                                    events_cv_.notify_all();

                                    if (worker_thread_.joinable()) {
                                        worker_thread_.join();
                                    }
                                    logger::Logger::info("Bus worker stopped");
                                }

                                DataBusMetricsSnapshot DataBus::get_metrics() const {
                                    size_t queue_size;
                                    {
                                        std::lock_guard<std::mutex> lock(events_mutex_);
                                        queue_size = events_queue_.size();
                                    }

                                    return DataBusMetricsSnapshot(metrics_, queue_size);
                                }

                                DataBus::~DataBus() {
                                    stop();
                                }

                                void DataBus::process_events() {
                                    while (running_.load()) {
                                        Event event;

                                        {
                                            std::unique_lock<std::mutex> lock(events_mutex_);
                                            events_cv_.wait(lock, [this]() {
                                                return !events_queue_.empty() || !running_.load();
                                            });

                                            if (!running_.load()) break;
                                            if (events_queue_.empty()) continue;

                                            event = events_queue_.front();
                                            events_queue_.pop();
                                            metrics_.queue_size = events_queue_.size();
                                        }

                                        handle_event(event);
                                        metrics_.events_processed++;
                                    }

                                    cleanup();
                                }

                                void DataBus::handle_event(const Event& event) {
                                    std::vector<Subscriber> subscribers;

                                    {
                                        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
                                        auto it = subscriptions_.find(event.type);
                                        if (it != subscriptions_.end()) {
                                            subscribers = it->second;
                                        }
                                    }

                                    if (event.data.contains("is_request") && event.data["is_request"] == true) {
                                        for (const auto& subscriber : subscribers) {
                                            try {
                                                subscriber.callback(event);
                                            } catch (const std::exception& e) {
                                                metrics_.handler_errors++;
                                            }
                                        }
                                    } else if (event.data.contains("correlation_id")) {
                                        std::string corr_id = event.data["correlation_id"];
                                        std::lock_guard<std::mutex> lock(requests_mutex_);
                                        auto it = pending_requests_.find(corr_id);
                                        if (it != pending_requests_.end()) {
                                            it->second.set_value(event.data);
                                            pending_requests_.erase(it);
                                        }
                                    } else {
                                        for (const auto& subscriber : subscribers) {
                                            try {
                                                subscriber.callback(event);
                                            } catch (const std::exception& e) {
                                                metrics_.handler_errors++;
                                                logger::Logger::warn(static_cast< const std::string&>(e.what()));
                                            }
                                        }
                                    }
                                }

                                std::future<nlohmann::json> DataBus::setup_response_waiter(const std::string& correlation_id) {
                                    std::lock_guard<std::mutex> lock(requests_mutex_);
                                    auto [it, inserted] = pending_requests_.emplace(correlation_id, std::promise<nlohmann::json>());
                                    return it->second.get_future();
                                }

                                void DataBus::cleanup() {
                                    std::lock_guard<std::mutex> lock(requests_mutex_);
                                    for (auto& [corr_id, promise] : pending_requests_) {
                                        try {
                                            promise.set_exception(std::make_exception_ptr(std::runtime_error("DataBus stopped")));
                                        } catch (...) {

                                        }
                                    }
                                    pending_requests_.clear();
                                }

                                std::string DataBus::generate_event_id() {
                                    return "evt_" + std::to_string(next_event_id_++);
                                }

                                std::string DataBus::generate_correlation_id() {
                                    return "corr_" + std::to_string(next_correlation_id_++);
                                }
