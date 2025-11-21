#ifndef ELASTICSEARCH_STRING_STORAGE_HPP
#define ELASTICSEARCH_STRING_STORAGE_HPP

#include <elasticsearch/client.hpp>
#include <elasticsearch/index.hpp>
#include <elasticsearch/document.hpp>
#include <string>
#include <vector>
#include <memory>

class ElasticStorage {
private:
    std::unique_ptr<elasticsearch::Client> client;
    std::string index_name;
    std::string host;
    int port;

    // Private methods
    std::string getCurrentTimestamp();
    bool createIndex();

public:
    // Constructor & Destructor
    ElasticStorage(const std::string& host = "localhost", 
                              int port = 9200,
                              const std::string& index = "string_data");
    ~ElasticStorage() = default;

    // Prevent copying
    ElasticStorage(const ElasticStorage&) = delete;
    ElasticStorage& operator=(const ElasticStorage&) = delete;

    // Allow moving
    ElasticStorage(ElasticStorage&&) = default;
    ElasticStorage& operator=(ElasticStorage&&) = default;

    // Storage methods
    bool storeString(const std::string& content, const std::string& document_type = "text");
    bool storeStringWithId(const std::string& id, const std::string& content, 
                          const std::string& document_type = "text");
    bool storeBulkStrings(const std::vector<std::string>& strings, 
                         const std::string& document_type = "text");

    // Search methods
    void searchStrings(const std::string& query);
    void searchStringsByField(const std::string& field, const std::string& value);

    // Utility methods
    bool isConnected() const;
    std::string getIndexName() const { return index_name; }
    std::string getConnectionInfo() const;

    // Index management
    bool deleteIndex();
    bool indexExists() const;
};

#endif // ELASTICSEARCH_STRING_STORAGE_HPP