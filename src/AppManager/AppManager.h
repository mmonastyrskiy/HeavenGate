#pragma once;
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


private:
    AppManager() = default;
    AppManager(const AppManager&) = delete;
    AppManager& operator=(const AppManager&) = delete;
    ~AppManager(){stop();}

};


