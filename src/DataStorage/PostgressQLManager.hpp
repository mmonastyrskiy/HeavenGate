
#pragma once
#include <memory>
#include <string>
#include <vector>
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
    bool insert_safely(pqxx::connection& conn, const std::string &table_name,std::vector<std::string>& values);
    bool insert_safely(pqxx::connection& conn, const std::string &table_name,std::vector<std::vector<std::string>>& values);
    bool PostgressQLManager::insert_safely_in_transaction(pqxx::work& txn,
                                                    const std::string& table_name,
                                                    std::vector<std::string>& values);

    bool bulk_insert_copy(pqxx::connection& conn, 
                                        const std::string& table_name,
                                        std::vector<std::vector<std::string>>& values);
     bool insert_safely(pqxx::connection& conn, 
                      const std::string& table_name,
                      std::vector<std::string>& values);
    
    bool insert_safely(pqxx::connection& conn, 
                      const std::string& table_name,
                      std::vector<std::vector<std::string>>& values);
    
    // Обновление
    bool safe_update(pqxx::connection& conn,
                    const std::string& table_name,
                    const std::vector<std::pair<std::string, std::string>>& set_values,
                    const std::vector<std::pair<std::string, std::string>>& where_conditions);
    
    bool safe_update(pqxx::connection& conn,
                    const std::string& table_name,
                    const std::vector<std::pair<std::pair<std::string, std::string>, 
                    std::vector<std::pair<std::string, std::string>>>>& updates);


bool PostgressQLManager::safe_delete(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::pair<std::string, std::string>>& where_conditions);
bool PostgressQLManager::safe_delete(pqxx::connection& conn,
                                  const std::string& table_name,
                                  const std::vector<std::vector<std::pair<std::string, std::string>>>& batch_conditions);

    

private:

    std::string build_connection_string();
    bool PostgressQLManager::is_valid_table_name(const std::string& table_name);
     // Вспомогательные функции
    bool insert_single(pqxx::work& txn, const std::string& table_name, 
                      std::vector<std::string>& values);
    bool update_single(pqxx::work& txn, const std::string& table_name,
                      const std::vector<std::pair<std::string, std::string>>& set_values,
                      const std::vector<std::pair<std::string, std::string>>& where_conditions);
    bool is_valid_table_name(const std::string& table_name);
    bool is_valid_column_name(const std::string& column_name);
    
    // Построение WHERE условия
    std::string build_where_clause(const std::vector<std::pair<std::string, std::string>>& conditions, 
                                  size_t start_param_index = 1);


    bool PostgressQLManager::delete_single(pqxx::work& txn,
                                    const std::string& table_name,
                                    const std::vector<std::pair<std::string, std::string>>& where_conditions);
};
