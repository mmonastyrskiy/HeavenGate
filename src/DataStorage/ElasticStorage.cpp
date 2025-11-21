#include "ElasticStorage.hpp"
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include "../../thirdparty/json.hpp"

// Callback function to write response data
size_t SimpleElasticsearchClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

SimpleElasticsearchClient::SimpleElasticsearchClient(const std::string& host, int port) 
    : host(host), port(port) {
    base_url = "http://" + host + ":" + std::to_string(port);
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

SimpleElasticsearchClient::~SimpleElasticsearchClient() {
    curl_global_cleanup();
}

std::string SimpleElasticsearchClient::httpRequest(const std::string& method, 
                                                 const std::string& endpoint, 
                                                 const std::string& body) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (!curl) {
        return "{\"error\": \"Failed to initialize CURL\"}";
    }

    std::string url = base_url + endpoint;
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Elasticsearch-CPP-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout
    
    // Set method-specific options
    if (method == "POST" || method == "PUT") {
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        } else {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        }
        
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return "{\"error\": \"" + std::string(curl_easy_strerror(res)) + "\"}";
    }
    
    return response;
}

bool SimpleElasticsearchClient::testConnection() {
    std::string response = httpRequest("GET", "/");
    return response.find("\"cluster_name\"") != std::string::npos;
}

std::string SimpleElasticsearchClient::getClusterInfo() {
    return httpRequest("GET", "/");
}

bool SimpleElasticsearchClient::createIndex(const std::string& index_name) {
    std::string response = httpRequest("PUT", "/" + index_name);
    return response.find("\"acknowledged\":true") != std::string::npos || 
           response.find("index_already_exists_exception") != std::string::npos;
}

bool SimpleElasticsearchClient::deleteIndex(const std::string& index_name) {
    std::string response = httpRequest("DELETE", "/" + index_name);
    return response.find("\"acknowledged\":true") != std::string::npos;
}

bool SimpleElasticsearchClient::indexExists(const std::string& index_name) {
    std::string response = httpRequest("HEAD", "/" + index_name);
    return response.empty(); // HEAD request returns empty body on success
}

bool SimpleElasticsearchClient::indexDocument(const std::string& index_name, 
                                            const std::string& id,
                                            const std::string& json_data) {
    std::string endpoint = "/" + index_name + "/_doc/" + id;
    std::string response = httpRequest("PUT", endpoint, json_data);
    return response.find("\"result\":\"created\"") != std::string::npos ||
           response.find("\"result\":\"updated\"") != std::string::npos;
}

bool SimpleElasticsearchClient::indexDocument(const std::string& index_name, 
                                            const std::string& json_data) {
    std::string endpoint = "/" + index_name + "/_doc";
    std::string response = httpRequest("POST", endpoint, json_data);
    return response.find("\"result\":\"created\"") != std::string::npos;
}

std::string SimpleElasticsearchClient::getDocument(const std::string& index_name, 
                                                 const std::string& id) {
    std::string endpoint = "/" + index_name + "/_doc/" + id;
    return httpRequest("GET", endpoint);
}

bool SimpleElasticsearchClient::deleteDocument(const std::string& index_name, 
                                             const std::string& id) {
    std::string endpoint = "/" + index_name + "/_doc/" + id;
    std::string response = httpRequest("DELETE", endpoint);
    return response.find("\"result\":\"deleted\"") != std::string::npos;
}

std::string SimpleElasticsearchClient::search(const std::string& index_name, 
                                            const std::string& query_json) {
    std::string endpoint = "/" + index_name + "/_search";
    return httpRequest("GET", endpoint, query_json);
}

std::string SimpleElasticsearchClient::searchAll(const std::string& query_json) {
    return httpRequest("GET", "/_search", query_json);
}

bool SimpleElasticsearchClient::bulkIndex(const std::string& bulk_data) {
    std::string response = httpRequest("POST", "/_bulk", bulk_data);
    return response.find("\"errors\":false") != std::string::npos;
}

std::string SimpleElasticsearchClient::getHealth() {
    return httpRequest("GET", "/_cluster/health");
}

std::string SimpleElasticsearchClient::getIndices() {
    return httpRequest("GET", "/_cat/indices?v");
}