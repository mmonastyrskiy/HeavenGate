#include "AppManager.h"
#include "AppComponent.h"
#include "../common/logger.h"

void AppManager::init(){
    logger::Logger::info("Initializing app");
components.push_back(AppComponent(AppComponentType::HG_DASHBOARD));
}

void AppManager::restart_component(const AppComponent& c){
    logger::Logger::info("Restarting " + c.name);
    AppComponentType t = c.type;
components.erase(std::remove(components.begin(),components.end(),c));
components.push_back(AppComponent(t));
}
void AppManager::stop(){
    logger::Logger::info("Stopping app! ");
    components.clear();
}

