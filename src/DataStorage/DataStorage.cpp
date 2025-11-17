#include "DataStorage.hpp"

// Include pqxx headers - adjust path based on your setup
//#include <pqxx/pqxx>
// OR if using local build:
// #include "path/to/your/pqxx/include/pqxx/pqxx"
#include "../../thirdparty/libpqxx/src/pqxx-source.hxx"
#include <string>
#include <cstdlib>
#include <memory>
#include "../common/Confparcer.h"
#include "../common/logger.h"

// Constructor
DataStorage::DataStorage() : connection(nullptr) {
}

// Destructor
DataStorage::~DataStorage() {
    if (connection && connection->is_open()) {
        connection->close();
    }
}

std::string DataStorage::build_connection_string() const {
    std::string conn_str;
    
    // Get configuration values
    std::string host = Confparcer::SETTING<std::string>("POSTGRE_HOST", "localhost");
    size_t port = Confparcer::SETTING<size_t>("POSTGRE_PORT", 5432);
    std::string dbname = Confparcer::SETTING<std::string>("POSTGRE_DBNAME", "postgres");
    std::string user = Confparcer::SETTING<std::string>("POSTGRE_USER", "postgres");
    
    // Build connection string
    conn_str = "host=" + host + " " +
               "port=" + std::to_string(port) + " " +
               "dbname=" + dbname + " " +
               "user=" + user + " ";
    
    // Get password from environment
    const char* password = std::getenv(PORTGRE_ENV);
    if (password == nullptr) {
        LOG_FATAL("Cannot retrieve postgres password: environment variable " + std::string(PORTGRE_ENV) + " is not set");
        throw std::runtime_error("Missing PostgreSQL password environment variable");
    }
    
    conn_str += "password=" + std::string(password);
    
    return conn_str;
}

void DataStorage::connect() {
    try {
        std::string conn_str = build_connection_string();
        connection = std::make_unique<pqxx::connection>(conn_str);
        
        if (connection->is_open()) {
            LOG_INFO("Connected to database: " + std::string(connection->dbname()));
        } else {
            LOG_ERROR("Cannot open database connection");
            throw std::runtime_error("Database connection failed");
        }
    } catch (const std::exception& e) {
        LOG_FATAL("Connection failed: " + std::string(e.what()));
        throw;
    }
}

bool DataStorage::is_connected() const {
    return connection && connection->is_open();
}

pqxx::connection& DataStorage::get_connection() {
    if (!is_connected()) {
        throw std::runtime_error("No active database connection");
    }
    return *connection;
}