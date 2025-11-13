
/*
 * Filename: d:\HeavenGate\src\DataBus\BusEvent.h
 * Path: d:\HeavenGate\src\DataBus
 * Created Date: Monday, November 3rd 2025, 5:39:44 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */
#pragma once
#include <string>
#include <chrono>
#include "../../thirdparty/json.hpp"

enum BusEventType {SERVICE_HEALTH_UPDATE,
                    REQUEST_CLASSIFIED,
                    REQUEST_PROCESSED,
                    SERVICE_REGISTERED,
                    REQUEST_ROUTED,
                    NEW_CLIENT_CONNECTION,
                    REQUEST_FOR_CLASSIFICATION

                };
struct Event {
    BusEventType type;
    std::string source;
    std::string id;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;
    std::string correlation_id;
};