#pragma once
#include <atomic>
#include <cstdint>

struct DataBusMetricsInternal {
    std::atomic<uint64_t> events_published{0};
    std::atomic<uint64_t> events_processed{0};
    std::atomic<uint64_t> events_dropped{0};
    std::atomic<uint64_t> handler_errors{0};
    std::atomic<uint64_t> queue_size{0};
    std::atomic<uint64_t> queue_overflow{0};
};

struct DataBusMetricsSnapshot {
    uint64_t events_published{0};
    uint64_t events_processed{0};
    uint64_t events_dropped{0};
    uint64_t handler_errors{0};
    uint64_t queue_size{0};
    uint64_t queue_overflow{0};
    
    DataBusMetricsSnapshot() = default;
    
    DataBusMetricsSnapshot(const DataBusMetricsInternal& internal, size_t current_queue_size) {
        events_published = internal.events_published.load();
        events_processed = internal.events_processed.load();
        events_dropped = internal.events_dropped.load();
        handler_errors = internal.handler_errors.load();
        queue_size = current_queue_size;
        queue_overflow = internal.queue_overflow.load();
    }
};