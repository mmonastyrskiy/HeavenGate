
// Include pqxx headers - adjust path based on your setup
#include <pqxx/pqxx>
// OR if using local build:
// #include "path/to/your/pqxx/include/pqxx/pqxx"
#include "../../thirdparty/libpqxx/src/pqxx-source.hxx"
#include <string>
#include <cstdlib>
#include <memory>
#include "../common/Confparcer.h"
#include "../common/logger.h"
#include "PostgressQLManager.hpp"
#include "PostgressQL_query.hpp"

// Constructor
PostgressQLManager::PostgressQLManager() : connection(nullptr) {
}

// Destructor
PostgressQLManager::~PostgressQLManager() {
    if (connection && connection->is_open()) {
        connection->close();
    }
}

std::string PostgressQLManager::build_connection_string() {
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
    const char* password = std::getenv(PORTGRE_ENV); //FIXME MAYBE A BETTER APPROACH?
    if (password == nullptr) {
        LOG_FATAL("Cannot retrieve postgres password: environment variable " + std::string(PORTGRE_ENV) + " is not set");
        throw std::runtime_error("Missing PostgreSQL password environment variable");
    }
    
    conn_str += "password=" + std::string(password);
    
    return conn_str;
}

void PostgressQLManager::connect() {
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

bool PostgressQLManager::is_connected() const {
    return connection && connection->is_open();
}

pqxx::connection& PostgressQLManager::get_connection() {
    if (!is_connected()) {
        LOG_ERROR("No actrive connections to postgres");
        throw std::runtime_error("No active database connection");
    }
    return *connection;
}
bool PostgressQLManager::create_table_safely(pqxx::connection &conn, const std::string &table_name){
    try {
        pqxx::work txn(conn);

        pqxx::result res = txn.exec_params("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = $1)",
        table_name
    );
    if(res[0][0].as<bool>()){
        LOG_INFO("Table " + table_name + " already exists");
        txn.commit();
        return false;
    }
        auto table = lookup_table(table_name);
        get_create_statement(table);

    txn.commit();


    }
    catch(const std::exception& e){
        LOG_ERROR("PSQL Error: " + std::string(e.what()));
    }
}
bool PostgressQLManager::insert_safely(pqxx::connection& conn, 
                                     const std::string& table_name,
                                     std::vector<std::string>& values) {
    auto cols = get_cols(lookup_table(table_name));
    if (cols.size() != values.size()) {
        LOG_ERROR("Number of columns (" + std::to_string(cols.size()) + 
                 ") doesn't match number of values (" + std::to_string(values.size()) + ")");
        return false;
    }
    
    try {
        pqxx::work txn(conn);
        
        // Build column list - must quote identifiers to handle case sensitivity
        std::string columns;
        for (size_t i = 0; i < cols.size(); ++i) {
            if (i > 0) columns += ", ";
            columns += "\"" + cols[i] + "\""; // Quote column names
        }
        
        // Build placeholders ($1, $2, $3...)
        std::string placeholders;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) placeholders += ", ";
            placeholders += "$" + std::to_string(i + 1);
        }
        
        // Table name cannot be parameterized, so we must validate it
        if (!is_valid_table_name(table_name)) {
            LOG_ERROR("Invalid table name: " + table_name);
            return false;
        }
        
        std::string query = "INSERT INTO \"" + table_name + "\" (" + 
                           columns + ") VALUES (" + placeholders + ")";
        
        // Convert vector to parameter list
        std::vector<std::string_view> params(values.begin(), values.end());
        
        pqxx::result res = txn.exec_params(query, 
                                          pqxx::prepare::make_dynamic_params(params));
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("PSQL Error: " + std::string(e.what()));
        return false;
    }
}

bool PostgressQLManager::insert_safely(pqxx::connection& conn, 
                                     const std::string& table_name,
                                     std::vector<std::vector<std::string>>& values) {
    try {
        pqxx::work txn(conn);
        
        for (auto& v : values) {
            if (!insert_safely_in_transaction(txn, table_name, v)) {
                txn.abort();
                return false;
            }
        }
        
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Batch insert PSQL Error: " + std::string(e.what()));
        return false;
    }
}

// Helper function for transaction-based inserts
bool PostgressQLManager::insert_safely_in_transaction(pqxx::work& txn,
                                                    const std::string& table_name,
                                                    std::vector<std::string>& values) {
    auto cols = get_cols(lookup_table(table_name));
    if (cols.size() != values.size()) {
        LOG_ERROR("Column/value count mismatch");
        return false;
    }
    
    try {
        // Build query (same logic as above)
        std::string columns;
        for (size_t i = 0; i < cols.size(); ++i) {
            if (i > 0) columns += ", ";
            columns += "\"" + cols[i] + "\"";
        }
        
        std::string placeholders;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) placeholders += ", ";
            placeholders += "$" + std::to_string(i + 1);
        }
        
        if (!is_valid_table_name(table_name)) {
            LOG_ERROR("Invalid table name: " + table_name);
            return false;
        }
        
        std::string query = "INSERT INTO \"" + table_name + "\" (" + 
                           columns + ") VALUES (" + placeholders + ")";
        
        std::vector<std::string_view> params(values.begin(), values.end());
        txn.exec_params(query, pqxx::prepare::make_dynamic_params(params));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Transaction insert error: " + std::string(e.what()));
        return false;
    }
}


bool PostgressQLManager::bulk_insert_copy(pqxx::connection& conn, 
                                        const std::string& table_name,
                                        std::vector<std::vector<std::string>>& values) {
    try {
        pqxx::work txn(conn);
        
        // Write data to string stream in COPY format
        std::stringstream data;
        for (const auto& row : values) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) data << "\t";
                data << row[i];
            }
            data << "\n";
        }
        
        txn.exec("COPY \"" + table_name + "\" FROM STDIN");
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("COPY insert error: " + std::string(e.what()));
        return false;
    }
}
bool PostgressQLManager::safe_update(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::pair<std::string, std::string>>& set_values,
                                  const std::vector<std::pair<std::string, std::string>>& where_conditions) {
    if (set_values.empty()) {
        LOG_ERROR("No SET values provided for UPDATE");
        return false;
    }
    
    if (where_conditions.empty()) {
        LOG_ERROR("No WHERE conditions provided for UPDATE (this is dangerous)");
        return false;
    }
    
    if (!is_valid_table_name(table_name)) {
        LOG_ERROR("Invalid table name: " + table_name);
        return false;
    }
    
    // Проверка валидности имен колонок
    for (const auto& set_pair : set_values) {
        if (!is_valid_column_name(set_pair.first)) {
            LOG_ERROR("Invalid column name in SET: " + set_pair.first);
            return false;
        }
    }
    
    for (const auto& where_pair : where_conditions) {
        if (!is_valid_column_name(where_pair.first)) {
            LOG_ERROR("Invalid column name in WHERE: " + where_pair.first);
            return false;
        }
    }
    
    try {
        pqxx::work txn(conn);
        
        // Построение SET части
        std::string set_clause;
        std::vector<std::string> all_params;
        
        for (size_t i = 0; i < set_values.size(); ++i) {
            if (i > 0) set_clause += ", ";
            set_clause += "\"" + set_values[i].first + "\" = $" + std::to_string(i + 1);
            all_params.push_back(set_values[i].second);
        }
        
        // Построение WHERE части
        std::string where_clause = build_where_clause(where_conditions, set_values.size() + 1);
        
        // Добавление WHERE параметров
        for (const auto& where_pair : where_conditions) {
            all_params.push_back(where_pair.second);
        }
        
        std::string query = "UPDATE \"" + table_name + "\" SET " + set_clause;
        if (!where_clause.empty()) {
            query += " WHERE " + where_clause;
        }
        
        txn.exec_params(query, pqxx::prepare::make_dynamic_params(all_params));
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("UPDATE PSQL Error: " + std::string(e.what()));
        return false;
    }
}

bool PostgressQLManager::safe_update(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::pair<std::pair<std::string, std::string>, 
                                  std::vector<std::pair<std::string, std::string>>>>& updates) {
    if (updates.empty()) {
        LOG_ERROR("No updates provided");
        return false;
    }
    
    try {
        pqxx::work txn(conn);
        
        for (const auto& update : updates) {
            const auto& set_value = update.first;
            const auto& where_conditions = update.second;
            
            if (!update_single(txn, table_name, {set_value}, where_conditions)) {
                txn.abort();
                return false;
            }
        }
        
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Batch UPDATE PSQL Error: " + std::string(e.what()));
        return false;
    }
}

// Вспомогательные методы реализации

bool PostgressQLManager::update_single(pqxx::work& txn, 
                                     const std::string& table_name,
                                     const std::vector<std::pair<std::string, std::string>>& set_values,
                                     const std::vector<std::pair<std::string, std::string>>& where_conditions) {
    if (set_values.empty() || where_conditions.empty()) {
        return false;
    }
    
    // Построение SET части
    std::string set_clause;
    std::vector<std::string> all_params;
    
    for (size_t i = 0; i < set_values.size(); ++i) {
        if (i > 0) set_clause += ", ";
        set_clause += "\"" + set_values[i].first + "\" = $" + std::to_string(i + 1);
        all_params.push_back(set_values[i].second);
    }
    
    // Построение WHERE части
    std::string where_clause = build_where_clause(where_conditions, set_values.size() + 1);
    
    // Добавление WHERE параметров
    for (const auto& where_pair : where_conditions) {
        all_params.push_back(where_pair.second);
    }
    
    std::string query = "UPDATE \"" + table_name + "\" SET " + set_clause;
    if (!where_clause.empty()) {
        query += " WHERE " + where_clause;
    }
    
    txn.exec_params(query, pqxx::prepare::make_dynamic_params(all_params));
    return true;
}

std::string PostgressQLManager::build_where_clause(const std::vector<std::pair<std::string, std::string>>& conditions, 
                                                 size_t start_param_index) {
    if (conditions.empty()) {
        return "";
    }
    
    std::string where_clause;
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) where_clause += " AND ";
        where_clause += "\"" + conditions[i].first + "\" = $" + std::to_string(start_param_index + i);
    }
    
    return where_clause;
}

bool PostgressQLManager::is_valid_column_name(const std::string& column_name) {
    if (column_name.empty()) return false;
    
    const std::vector<std::string> dangerous_patterns = {
        ";", "--", "/*", "*/", "DROP", "DELETE", "UPDATE", "INSERT", "SELECT",
        "WHERE", "SET", "FROM", "UNION", "OR", "AND", "="
    };
    
    for (const auto& pattern : dangerous_patterns) {
        if (column_name.find(pattern) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool PostgressQLManager::safe_delete(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::pair<std::string, std::string>>& where_conditions) {
    if (where_conditions.empty()) {
        LOG_ERROR("No WHERE conditions provided for DELETE (this would delete all records)");
        return false;
    }
    
    if (!is_valid_table_name(table_name)) {
        LOG_ERROR("Invalid table name: " + table_name);
        return false;
    }
    
    // Проверка валидности имен колонок в WHERE условиях
    for (const auto& where_pair : where_conditions) {
        if (!is_valid_column_name(where_pair.first)) {
            LOG_ERROR("Invalid column name in WHERE: " + where_pair.first);
            return false;
        }
    }
    
    try {
        pqxx::work txn(conn);
        
        // Построение WHERE части
        std::string where_clause = build_where_clause(where_conditions);
        
        // Сбор значений для параметров
        std::vector<std::string> params;
        for (const auto& where_pair : where_conditions) {
            params.push_back(where_pair.second);
        }
        
        std::string query = "DELETE FROM \"" + table_name + "\" WHERE " + where_clause;
        
        txn.exec_params(query, pqxx::prepare::make_dynamic_params(params));
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("DELETE PSQL Error: " + std::string(e.what()));
        return false;
    }
}

bool PostgressQLManager::safe_delete(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::vector<std::pair<std::string, std::string>>>& batch_conditions) {
    if (batch_conditions.empty()) {
        LOG_ERROR("No delete conditions provided");
        return false;
    }
    
    try {
        pqxx::work txn(conn);
        
        for (const auto& where_conditions : batch_conditions) {
            if (where_conditions.empty()) {
                LOG_ERROR("Empty WHERE conditions in batch delete");
                txn.abort();
                return false;
            }
            
            if (!delete_single(txn, table_name, where_conditions)) {
                txn.abort();
                return false;
            }
        }
        
        txn.commit();
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Batch DELETE PSQL Error: " + std::string(e.what()));
        return false;
    }
}

bool PostgressQLManager::delete_single(pqxx::work& txn,
                                    const std::string& table_name,
                                    const std::vector<std::pair<std::string, std::string>>& where_conditions) {
    if (where_conditions.empty()) {
        return false;
    }
    
    // Построение WHERE части
    std::string where_clause = build_where_clause(where_conditions);
    
    // Сбор значений для параметров
    std::vector<std::string> params;
    for (const auto& where_pair : where_conditions) {
        params.push_back(where_pair.second);
    }
    
    std::string query = "DELETE FROM \"" + table_name + "\" WHERE " + where_clause;
    
    txn.exec_params(query, pqxx::prepare::make_dynamic_params(params));
    return true;
}