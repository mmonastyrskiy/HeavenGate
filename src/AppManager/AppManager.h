#include <string>
#include <vector>
#include <memory>
#include "AppComponent.h"

class AppManager {
private:
    std::vector<std::unique_ptr<AppComponent>> components;
    
public:
    void start_all();
    void stop_all();
    void restart_component(const std::string& name);
    bool is_component_running(const std::string& name);
};