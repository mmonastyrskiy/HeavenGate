#include "ElasticStorage.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

// Constructor
ElasticStorage::ElasticStorage(const std::string& host, 
                                                     int port,
                                                     const std::string& index)
    : host(host), port(port), index_name(index) 
{
    try {
        client = std::make_unique<elasticsearch::Client>(host, port);
        std::cout << "Connected to Elasticsearch at " << host << ":" << port << std::endl;
        
        // Create index if it doesn't exist
        if (!createIndex()) {
            std::cerr << "Warning: Failed to create index '" << index_name << "'" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to Elasticsearch: " << e.what() << std::endl;
        throw;
    }
}

// Create Elasticsearch index
bool ElasticsearchStringStorage::createIndex() {
    try {
        elasticsearch::Index index(*client, index_name);
        auto response = index.create();
        
        if (response.status() == 200) {
            std::cout << "Index '" << index_name << "' created successfully" << std::endl;
            return true;
        } else if (response.status() == 400) {
            std::cout << "Index '" << index_name << "' already exists" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to create index. Status: " << response.status() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating index: " << e.what() << std::endl;
        return false;
    }
}

// Store a simple string with automatic ID generation
bool ElasticsearchStringStorage::storeString(const std::string& content, 
                                           const std::string& document_type) {
    try {
        // Create JSON document
        nlohmann::json document;
        document["content"] = content;
        document["timestamp"] = getCurrentTimestamp();
        document["length"] = content.length();
        document["type"] = document_type;

        // Index the document
        elasticsearch::Document doc(*client, index_name, document_type);
        auto response = doc.index(document.dump());

        if (response.status() == 201) {
            auto response_json = nlohmann::json::parse(response.body());
            std::cout << "String stored successfully! ID: " 
                     << response_json["_id"] << std::endl;
            return true;
        } else {
            std::cerr << "Failed to store string. Status: " << response.status() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error storing string: " << e.what() << std::endl;
        return false;
    }
}

// Store a string with custom ID
bool ElasticsearchStringStorage::storeStringWithId(const std::string& id, 
                                                 const std::string& content, 
                                                 const std::string& document_type) {
    try {
        nlohmann::json document;
        document["content"] = content;
        document["timestamp"] = getCurrentTimestamp();
        document["length"] = content.length();
        document["type"] = document_type;

        elasticsearch::Document doc(*client, index_name, document_type);
        auto response = doc.index(document.dump(), id);

        if (response.status() == 201) {
            std::cout << "String stored with custom ID: " << id << std::endl;
            return true;
        } else {
            std::cerr << "Failed to store string with ID. Status: " << response.status() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error storing string with ID: " << e.what() << std::endl;
        return false;
    }
}

// Store multiple strings in bulk
bool ElasticsearchStringStorage::storeBulkStrings(const std::vector<std::string>& strings, 
                                                const std::string& document_type) {
    try {
        std::string bulk_data;
        
        for (const auto& str : strings) {
            // Action and metadata
            nlohmann::json action;
            action["index"] = nlohmann::json::object();
            action["index"]["_index"] = index_name;
            action["index"]["_type"] = document_type;
            
            // Document data
            nlohmann::json document;
            document["content"] = str;
            document["timestamp"] = getCurrentTimestamp();
            document["length"] = str.length();
            document["type"] = document_type;

            bulk_data += action.dump() + "\n";
            bulk_data += document.dump() + "\n";
        }

        auto response = client->post("/_bulk", bulk_data);
        
        if (response.status() == 200) {
            std::cout << "Bulk insert completed for " << strings.size() << " strings" << std::endl;
            return true;
        } else {
            std::cerr << "Bulk insert failed. Status: " << response.status() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in bulk insert: " << e.what() << std::endl;
        return false;
    }
}

// Search for strings containing specific text
void ElasticsearchStringStorage::searchStrings(const std::string& query) {
    try {
        nlohmann::json search_query;
        search_query["query"] = nlohmann::json::object();
        search_query["query"]["match"] = nlohmann::json::object();
        search_query["query"]["match"]["content"] = query;

        auto response = client->post("/" + index_name + "/_search", search_query.dump());
        
        if (response.status() == 200) {
            auto results = nlohmann::json::parse(response.body());
            int total_hits = results["hits"]["total"]["value"];
            
            std::cout << "Found " << total_hits << " results for query: '" << query << "'" << std::endl;
            
            for (const auto& hit : results["hits"]["hits"]) {
                std::cout << "ID: " << hit["_id"] 
                         << " | Content: " << hit["_source"]["content"] 
                         << " | Score: " << hit["_score"] << std::endl;
            }
        } else {
            std::cerr << "Search failed. Status: " << response.status() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching: " << e.what() << std::endl;
    }
}

// Search by specific field
void ElasticsearchStringStorage::searchStringsByField(const std::string& field, 
                                                    const std::string& value) {
    try {
        nlohmann::json search_query;
        search_query["query"] = nlohmann::json::object();
        search_query["query"]["term"] = nlohmann::json::object();
        search_query["query"]["term"][field] = value;

        auto response = client->post("/" + index_name + "/_search", search_query.dump());
        
        if (response.status() == 200) {
            auto results = nlohmann::json::parse(response.body());
            int total_hits = results["hits"]["total"]["value"];
            
            std::cout << "Found " << total_hits << " results for " << field << ": '" << value << "'" << std::endl;
            
            for (const auto& hit : results["hits"]["hits"]) {
                std::cout << "ID: " << hit["_id"] 
                         << " | Content: " << hit["_source"]["content"] 
                         << " | Type: " << hit["_source"]["type"] << std::endl;
            }
        } else {
            std::cerr << "Field search failed. Status: " << response.status() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in field search: " << e.what() << std::endl;
    }
}

// Check if connected to Elasticsearch
bool ElasticsearchStringStorage::isConnected() const {
    try {
        auto response = client->get("/");
        return response.status() == 200;
    } catch (const std::exception&) {
        return false;
    }
}

// Delete index
bool ElasticsearchStringStorage::deleteIndex() {
    try {
        elasticsearch::Index index(*client, index_name);
        auto response = index.delete_();
        
        if (response.status() == 200) {
            std::cout << "Index '" << index_name << "' deleted successfully" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to delete index. Status: " << response.status() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error deleting index: " << e.what() << std::endl;
        return false;
    }
}

// Check if index exists
bool ElasticsearchStringStorage::indexExists() const {
    try {
        auto response = client->head("/" + index_name);
        return response.status() == 200;
    } catch (const std::exception&) {
        return false;
    }
}

// Get connection information
std::string ElasticsearchStringStorage::getConnectionInfo() const {
    std::stringstream ss;
    ss << "Elasticsearch: " << host << ":" << port << ", Index: " << index_name;
    return ss.str();
}

// Get current timestamp
std::string ElasticsearchStringStorage::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}