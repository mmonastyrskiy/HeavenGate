#include <chrono>
#include <string>
#include "../thirdparty/json.hpp"
using namespace std::chrono_literals;
class DataBus {
    using EventCallback = std::function<void(const BusEventType&)>;
    using SubscriptionId = uint64_t;
    
    public:
    static DataBus& instance();
    void publish(BusEventType type,std::string& source, const nlohmann::json& data);
    SubscriptionId subscribe(BusEventType type, EventCallback callback);
    void unsubscribe(SubscriptionId id);

     nlohmann::json request(BusEventType type, 
                          const nlohmann::json& data,
                          std::chrono::milliseconds timeout = 1s);
};