#include <string>
#include <atomic>
#include <chrono>
#include <DataBus.h>

class LoadBalancer
{
private:
    LoadBalancer() = default;

    ~LoadBalancer();
public:
struct BackendNode{
    std::string id;
    std::string host;
    int port;
    bool is_honeypot;
    float weight;
    std::atomic<int> current_clients {0};
    std::atomic<bool> is_healthy{true};
    std::chrono::steady_clock::time_point last_health_check;

    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_responses{0};
    std::atomic<uint64_t> failed_responses{0};
    std::atomic<uint64_t> total_response_time_ms{0};
    std::chrono::steady_clock::time_point last_request_time;



};


};
