#pragma GCC diagnostic ignored "-Wc++17-extensions"


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

#include "../../thirdparty/json.hpp"

#include "BusEvent.h"
#include "subscriptionID.h"
#include "DataBusMetrics.h"

class DataBus {
public:
static constexpr size_t MAX_QUEUE_SIZE = 10000; // TODO: Move to config
    static DataBus& instance() {
        static DataBus instance;
        return instance;
    }

    // Публикация событий
    void publish(BusEventType type, const std::string& source, const nlohmann::json& data) {
        Event event;
        event.type = type;
        event.source = source;
        event.data = data;
        event.timestamp = std::chrono::system_clock::now();
        event.id = generate_event_id();
        
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            events_queue_.push(event);
            metrics_.queue_size = events_queue_.size();
            if(metrics_.queue_size >= MAX_QUEUE_SIZE){
                events_queue_.pop();
                metrics_.queue_overflow++;
            }
        }
        events_cv_.notify_one();
        
        // Обновление метрик
        metrics_.events_published++;
    }

    // Подписка на события
    SubscriptionId subscribe(BusEventType type, EventCallback callback) {
        SubscriptionId id = next_subscription_id_++;
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_[type].push_back({id, callback});
        }
        
        return id;
    }

    // Отписка от событий
    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        for (auto& [type, subscribers] : subscriptions_) {
            subscribers.erase(
                std::remove_if(subscribers.begin(), subscribers.end(),
                    [id](const auto& sub) { return sub.id == id; }),
                subscribers.end()
            );
        }
    }

    // Синхронный запрос-ответ
    nlohmann::json request(BusEventType type, const nlohmann::json& data, 
                          std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
        std::string correlation_id = generate_correlation_id();
        auto response_future = setup_response_waiter(correlation_id);
        
        // Публикуем запрос
        publish(type, "requestor", {
            {"data", data},
            {"correlation_id", correlation_id},
            {"is_request", true}
        });
        
        // Ждем ответ
        if (response_future.wait_for(timeout) == std::future_status::ready) {
            return response_future.get();
        }
        
        // Таймаут - очищаем ожидание
        {
            std::lock_guard<std::mutex> lock(requests_mutex_);
            pending_requests_.erase(correlation_id);
        }
        
        throw std::runtime_error("Request timeout");
    }

    // Запуск шины
    void start() {
        if (running_.exchange(true)) return;
        
        worker_thread_ = std::thread(&DataBus::process_events, this);
    }

    // Остановка шины
    void stop() {
        if (!running_.exchange(false)) return;
        
        events_cv_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    // Получение метрик
    DataBusMetricsSnapshot get_metrics() const {
        size_t queue_size;
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            queue_size = events_queue_.size();
        }
        
        return DataBusMetricsSnapshot(metrics_, queue_size);
    }

private:
    DataBus() = default;
    ~DataBus() { stop(); }

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

    void process_events() {
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
            
            // Обработка события
            handle_event(event);
            metrics_.events_processed++;
        }
        
        // Очистка оставшихся событий при остановке
        cleanup();
    }

    void handle_event(const Event& event) {
        std::vector<Subscriber> subscribers;
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            auto it = subscriptions_.find(event.type);
            if (it != subscriptions_.end()) {
                subscribers = it->second;
            }
        }
        
        // Обработка механизма запрос-ответ
        if (event.data.contains("is_request") && event.data["is_request"] == true) {
            // Это запрос - ищем обработчиков
            for (const auto& subscriber : subscribers) {
                try {
                    subscriber.callback(event);
                } catch (const std::exception& e) {
                    metrics_.handler_errors++;
                }
            }
        } else if (event.data.contains("correlation_id")) {
            // Это ответ - завершаем ожидание
            std::string corr_id = event.data["correlation_id"];
            std::lock_guard<std::mutex> lock(requests_mutex_);
            auto it = pending_requests_.find(corr_id);
            if (it != pending_requests_.end()) {
                it->second.set_value(event.data);
                pending_requests_.erase(it);
            }
        } else {
            // Обычное событие
            for (const auto& subscriber : subscribers) {
                try {
                    subscriber.callback(event);
                } catch (const std::exception& e) {
                    metrics_.handler_errors++;
                }
            }
        }
    }

    std::future<nlohmann::json> setup_response_waiter(const std::string& correlation_id) {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        auto [it, inserted] = pending_requests_.emplace(correlation_id, std::promise<nlohmann::json>());
        return it->second.get_future();
    }

    void cleanup() {
        // Завершаем все ожидающие запросы при остановке
        std::lock_guard<std::mutex> lock(requests_mutex_);
        for (auto& [corr_id, promise] : pending_requests_) {
            try {
                promise.set_exception(std::make_exception_ptr(std::runtime_error("DataBus stopped")));
            } catch (...) {
                // Игнорируем исключения при установке исключения
            }
        }
        pending_requests_.clear();
    }

    std::string generate_event_id() {
        return "evt_" + std::to_string(next_event_id_++);
    }

    std::string generate_correlation_id() {
        return "corr_" + std::to_string(next_correlation_id_++);
    }
};