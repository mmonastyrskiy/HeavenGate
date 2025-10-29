#pragma once
#include <cstdint>
#include <functional>
using SubscriptionId = uint64_t;
using EventCallback = std::function<void(const Event&)>;
