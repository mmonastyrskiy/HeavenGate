/*
 * Filename: d:\HeavenGate\src\API\dashboardAPI.cpp
 * Path: d:\HeavenGate\src\API
 * Created Date: Saturday, November 8th 2025, 8:50:32 am
 * Author: mmonastyrskiy
 * 
 * Copyright (c) 2025 Your Company
 */

#include "dashboardAPI.h"
#include "dashboardAPIreply.hpp"
#include <curl/curl.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <optional>
#include "../common/logger.h"
#include "../common/Confparcer.h"
#include "../DataStorage/PostgressQLManager.hpp"
#include "../common/generic.h"
#include "../User/User.hpp"
#include <DataSecurity.hpp>



// Callback function for cURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Helper function to get current time in ISO format
static std::string getCurrentTimeISO() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

// Singleton implementation
DashboardAPI& DashboardAPI::the() {
    static DashboardAPI instance;
    return instance;
}



std::string DashboardAPI::callClientChange(const int& real_size, const int& malicious_size, int* err){
 
    
    CURL* curl;
    CURLcode res;
    std::string response;
    
    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if(!curl) {
        if(err) *err = -1;
        curl_global_cleanup();
        return "";
    }
    
    // Form the full URL - use the class member with proper scope
    std::string url = DashboardAPI::baseUrl + "/user_registered";

    std::string currentTime = getCurrentTimeISO();
    
    // Form JSON data
    std::string jsonData = "{"
                      "\"legitClients\":" + std::to_string(real_size) + ","
                      "\"maliciousClients\":" + std::to_string(malicious_size) + ""
                      "}";
    auto showRequests = Confparcer::SETTING<bool>("SHOW_REQ_LOG",true);
    if(showRequests){
    LOG_INFO("Sending JSON: " + jsonData);
    LOG_INFO("URL: " + url);
    }
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    // Configure cURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Execute request
    res = curl_easy_perform(curl);
    
    // Handle errors
    if(res != CURLE_OK) {
        std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        if(err) *err = static_cast<int>(res);
        response = "";
        LOG_WARN("Error connecting to the dashboard");
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if(http_code != 200) {
            std::cerr << "HTTP error: " << http_code << std::endl;
            if(err) *err = static_cast<int>(http_code);
        } else {
            if(err) *err = 0;
        }
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    return response;
}


std::string DashboardAPI::callAgentChange(const int& real_size, const int& honey_size, int* err){

    CURL* curl;
    CURLcode res;
    std::string response;
    
    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if(!curl) {
        if(err) *err = -1;
        curl_global_cleanup();
        return "";
    }
    
    // Form the full URL - use the class member with proper scope
    std::string url = DashboardAPI::baseUrl + "/agents";

    std::string currentTime = getCurrentTimeISO();
    
    // Form JSON data
    std::string jsonData = "{"
                      "\"realServers\":" + std::to_string(real_size) + ","
                      "\"honeypots\":" + std::to_string(honey_size) + ""
                      "}";
    auto showRequests = Confparcer::SETTING<bool>("SHOW_REQ_LOG",true);
    if(showRequests){
    LOG_INFO("Sending JSON: " + jsonData);
    LOG_INFO("URL: " + url);
    }
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    // Configure cURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Execute request
    res = curl_easy_perform(curl);
    
    // Handle errors
    if(res != CURLE_OK) {
        std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        if(err) *err = static_cast<int>(res);
        response = "";
        LOG_WARN("Error connecting to the dashboard");
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if(http_code != 200) {
            std::cerr << "HTTP error: " << http_code << std::endl;
            if(err) *err = static_cast<int>(http_code);
        } else {
            if(err) *err = 0;
        }
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    return response;
}


// Main API method implementation
std::string DashboardAPI::callRequestRegistered(const std::string& client_ip, 
                                           const std::string& server_id, 
                                           bool is_malicious,
                                           int* err) {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if(!curl) {
        if(err) *err = -1;
        curl_global_cleanup();
        return "";
    }
    
    // Form the full URL - use the class member with proper scope
    std::string url = DashboardAPI::baseUrl + "/req_registered";

    std::string currentTime = getCurrentTimeISO();
    
    // Form JSON data
    std::string jsonData = "{"
                          "\"ClientIP\":\"" + client_ip + "\","
                          "\"Path\":\"" + server_id + "\","
                          "\"IsMalicious\":" + (is_malicious ? "true" : "false") + ","
                          "\"Timestamp\":\"" + currentTime + "\""
                          "}";
    auto showRequests = Confparcer::SETTING<bool>("SHOW_REQ_LOG",true);
    if(showRequests){
    LOG_INFO("Sending JSON: " + jsonData);
    LOG_INFO("URL: " + url);
    }
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    // Configure cURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.length());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Execute request
    res = curl_easy_perform(curl);
    
    // Handle errors
    if(res != CURLE_OK) {
        std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        if(err) *err = static_cast<int>(res);
        response = "";
        LOG_WARN("Error connecting to the dashboard");
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if(http_code != 200) {
            std::cerr << "HTTP error: " << http_code << std::endl;
            if(err) *err = static_cast<int>(http_code);
        } else {
            if(err) *err = 0;
        }
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    return response;
}
std::string DashboardAPI::callGetClients(std::string& token){
    int e {0};
    if(!std::all_of(token.begin(),token.end(),::isalnum)){
        e=1;
        return ret_401(e);
    }
    std::optional<Userland::User> tu = Userland::User::load_web_token(token,&e);
    if(!tu){
        return ret_401(e);
    }
    
    Userland::User& u = tu.value();  // Use reference to avoid copy
    
    if(!u.hasPermission(Userland::User::Permissions::VIEW_CLIENTS)) {
        return ret_403("You have no permission to view clients");
    }
    
    try {
        PostgressQLManager pgm;
        if(!pgm.is_connected()){
            pgm.connect();
            if(!pgm.is_connected()) {
                return ret_500("Database connection failed");
            }
        }
        
        auto& conn = pgm.get_connection();
        std::string data = pgm.select_where(conn, "clients", "is_active = TRUE");
        
        if(u.hasPermission(Userland::User::Permissions::PERSONAL_DATA_VIEW)){
            return ret_200(data);
        } else {
            return ret_200(utils::MaskIPs(data));
        }
    } catch(const std::exception& e) {
        // Log the error
        return ret_500("Database error");
    }
}


