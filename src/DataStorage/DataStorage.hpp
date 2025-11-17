#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <pqxx/pqxx>
#include <string>
#include <memory>

class DataStorage
{
private:
    const char* PORTGRE_ENV = "HG_DBPASS";
    std::unique_ptr<pqxx::connection> connection;

public:
    // Constructor & Destructor
    DataStorage();
    ~DataStorage();
    
    // Delete copy constructor and assignment operator
    DataStorage(const DataStorage& stor) = delete;
    DataStorage& operator=(const DataStorage& ds) = delete;
    
    // Move constructor and move assignment operator
    DataStorage(DataStorage&& other) noexcept = default;
    DataStorage& operator=(DataStorage&& other) noexcept = default;

    // Public methods
    void connect();
    bool is_connected() const;
    pqxx::connection& get_connection();
    
    // Optional: Method to get connection info (without password)
    std::string get_connection_info() const;

private:
    // Helper method to construct connection string
    std::string build_connection_string(const char* db_password) const;
};

#endif // DATASTORAGE_H
