/*
 * Filename: d:\HeavenGate\src\AppManager\AppManager.h
 * Path: d:\HeavenGate\src\AppManager
 * Created Date: Tuesday, November 11th 2025, 8:38:24 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#pragma once
#include <filesystem>
#include "AppComponent.h"
#include <vector>

class AppManager
{

public:
int e {0};
std::vector<AppComponent> components;


void init();
void stop();
void restart_component(const AppComponent& c);
static AppManager& the();


private:
    AppManager() = default;
    AppManager(const AppManager&) = delete;
    AppManager& operator=(const AppManager&) = delete;
    ~AppManager(){stop();}

};


