#pragma once
#include <string>
#include <chrono>
#include "../../thirdparty/json.hpp"

enum BusEventType {SERVICE_HEALTH_UPDATE,
                    REQUEST_CLASSIFIED,
                    REQUEST_PROCESSED,
                    SERVICE_REGISTERED,
                    REQUEST_ROUTED,

                };
struct Event {
    BusEventType type;
    std::string source;
    std::string id;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;
    std::string correlation_id;
};