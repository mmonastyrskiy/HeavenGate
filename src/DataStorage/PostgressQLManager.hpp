
#pragma once
#include <memory>
#include <string>
// Forward declaration
namespace pqxx {
    class connection;
}

class PostgressQLManager {
private:
    static constexpr const char* PORTGRE_ENV = "HG_DBPASS";
    std::unique_ptr<pqxx::connection> connection;

public:
    PostgressQLManager();
    ~PostgressQLManager();
    
    // Delete copy operations
    PostgressQLManager(const PostgressQLManager&) = delete;
    PostgressQLManager& operator=(const PostgressQLManager&) = delete;
    
    // Allow move operations
    PostgressQLManager(PostgressQLManager&&) = default;
    PostgressQLManager& operator=(PostgressQLManager&&) = default;

    // Public methods
    void connect();
    bool is_connected() const;
    pqxx::connection& get_connection();
    bool create_table_safely(pqxx::connection &conn, const std::string &table_name);

private:
    std::string build_connection_string() const;
};
