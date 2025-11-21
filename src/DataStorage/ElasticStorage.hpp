#ifndef SIMPLE_ELASTICSEARCH_CLIENT_HPP
#define SIMPLE_ELASTICSEARCH_CLIENT_HPP

#include <string>
#include <map>
#include <memory>

class SimpleElasticsearchClient {
private:
    std::string host;
    int port;
    std::string base_url;
    
    std::string httpRequest(const std::string& method, 
                          const std::string& endpoint, 
                          const std::string& body = "");
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

public:
    SimpleElasticsearchClient(const std::string& host = "localhost", int port = 9200);
    ~SimpleElasticsearchClient();
    
    // Connection methods
    bool testConnection();
    std::string getClusterInfo();
    
    // Index operations
    bool createIndex(const std::string& index_name);
    bool deleteIndex(const std::string& index_name);
    bool indexExists(const std::string& index_name);
    
    // Document operations
    bool indexDocument(const std::string& index_name, 
                      const std::string& id,
                      const std::string& json_data);
    bool indexDocument(const std::string& index_name, 
                      const std::string& json_data); // auto-generate ID
    std::string getDocument(const std::string& index_name, 
                           const std::string& id);
    bool deleteDocument(const std::string& index_name, 
                       const std::string& id);
    
    // Search operations
    std::string search(const std::string& index_name, 
                      const std::string& query_json);
    std::string searchAll(const std::string& query_json);
    
    // Bulk operations
    bool bulkIndex(const std::string& bulk_data);
    
    // Utility methods
    std::string getHealth();
    std::string getIndices();
};

#endif // SIMPLE_ELASTICSEARCH_CLIENT_HPP