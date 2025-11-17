#include "DataStorage.hpp"
#include <string>
#include <cstdlib> 
#include <memory>
#include "../common/Confparcer.h"
#include "../common/logger.h"

void DataStorage::connect() {
    std::string conn_str;
    
    // Build proper PostgreSQL connection string
    conn_str = "host=" + Confparcer::SETTING<std::string>("POSTGRE_HOST", "localhost") + " " +
               "port=" + std::to_string(Confparcer::SETTING<size_t>("POSTGRE_PORT", 5432)) + " " +
               "dbname=" + Confparcer::SETTING<std::string>("POSTGRE_DBNAME", "postgres") + " " +
               "user=" + Confparcer::SETTING<std::string>("POSTGRE_USER", "postgres") + " ";

    const char* PASSWD = std::getenv(PORTGRE_ENV);
    if (PASSWD == nullptr) {
        LOG_FATAL("Cannot retrieve postgres password: the env variable " + std::string(PORTGRE_ENV) + " is not set");
        throw std::runtime_error("Missing PostgreSQL password environment variable");
    }
    
    conn_str.append("password=" + std::string(PASSWD));

    // Optional: Add connection timeout and other options
    conn_str.append(" connect_timeout=10");

    try {
        connection = std::make_unique<pqxx::connection>(conn_str);
        
        if (connection->is_open()) {
            LOG_INFO("Connected to database: " + std::string(connection->dbname()));
        } else {
            LOG_ERROR("Can't open database");
            throw std::runtime_error("Database connection failed");
        }
    } catch (const std::exception& e) {
        LOG_FATAL("Connection failed: " + std::string(e.what()));
        throw; // Re-throw to maintain error propagation
    }
}