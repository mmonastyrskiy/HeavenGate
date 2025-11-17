#ifndef DATASTORAGE_HPP
#define DATASTORAGE_HPP

#include <memory>
#include <string>

// Forward declaration
namespace pqxx {
    class connection;
}

class DataStorage {
private:
    static constexpr const char* PORTGRE_ENV = "HG_DBPASS";
    std::unique_ptr<pqxx::connection> connection;

public:
    DataStorage();
    ~DataStorage();
    
    // Delete copy operations
    DataStorage(const DataStorage&) = delete;
    DataStorage& operator=(const DataStorage&) = delete;
    
    // Allow move operations
    DataStorage(DataStorage&&) = default;
    DataStorage& operator=(DataStorage&&) = default;

    // Public methods
    void connect();
    bool is_connected() const;
    pqxx::connection& get_connection();

private:
    std::string build_connection_string() const;
};

#endif // DATASTORAGE_HPP