/*
 * Filename: d:\HeavenGate\src\AppManager\AppManager.cpp
 * Path: d:\HeavenGate\src\AppManager
 * Created Date: Tuesday, November 11th 2025, 8:38:24 pm
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#include "AppManager.h"
#include "AppComponent.h"
#include "../common/logger.h"
#include <algorithm>

void AppManager::init(){
    logger::Logger::info("Initializing app");
components.push_back(AppComponent(AppComponentType::HG_DASHBOARD));
}

void AppManager::restart_component(const AppComponent& c){
    logger::Logger::info("Restarting " + c.name);
    AppComponentType t = c.type;
components.erase(std::remove_if(components.begin(),components.end(),
[&c](const AppComponent& comp){return comp.name == c.name;}
));
components.push_back(AppComponent(t));
}
void AppManager::stop(){
    logger::Logger::info("Stopping app! ");
    components.clear();
}

AppManager& AppManager::the(){
    static AppManager instance;
    return instance;

}
