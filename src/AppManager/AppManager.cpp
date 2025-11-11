#include "AppManager.h"
#include "AppComponent.h"
#include "../common/logger.h"
#include "../common/generic.h"
#if ISLINUX
#include <unistd.h>
#else
#include <windows.h>
#endif

void AppManager::start_all() {
    // Создаем и запускаем все компоненты
    components.push_back(std::make_unique<AppComponent>(AppComponentType::HG_DASHBOARD));
    
    // Запускаем каждый компонент
    for (auto& comp : components) {
        if (comp->run()) {
            logger::Logger::info("Компонент " + comp->name + " запущен");
        } else {
            logger::Logger::err("Не удалось запустить " + comp->name);
        }
    }
}

void AppManager::stop_all() {
    for (auto& comp : components) {
        comp->stop();
    }
}

void AppManager::restart_component(const std::string& name) {
    for (auto& comp : components) {
        if (comp->name == name) {
            comp->stop();
            #if ISLINUX
            sleep(1); // Даем время на завершение
            #else
            Sleep(1);
            #endif
            comp->run();
            break;
        }
    }
}

bool AppManager::is_component_running(const std::string& name) {
    for (auto& comp : components) {
        if (comp->name == name) {
            return comp->isRunning();
        }
    }
    return false;
}