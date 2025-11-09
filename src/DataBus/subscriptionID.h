
/*
 * Filename: d:\HeavenGate\src\DataBus\subscriptionID.h
 * Path: d:\HeavenGate\src\DataBus
 * Created Date: Wednesday, October 29th 2025, 10:03:43 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once
#include <cstdint>
#include <functional>
using SubscriptionId = uint64_t;
using EventCallback = std::function<void(const Event&)>;
