#include "ReqParser.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include "../common/rand.h"
#include "../common/logger.h"

namespace Classifier {

HttpRequestParser::ParsedRequest HttpRequestParser::parse(const std::string& uid,const std::string& request_data) {
    ParsedRequest result;
    RandomGenerator rg;
    result.ReqID = rg.generate(15);
    result.uid = uid;


    LOG_INFO("Reqest was assigned an ID: " + result.ReqID);
    
    std::istringstream stream(request_data);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        parseRequestLine(line, result);
    }
    
    // Parse headers
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        if (line.back() == '\r') line.pop_back();
        parseHeader(line, result);
    }
    
    // Parse body (if present)
    size_t header_end = request_data.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        result.body = request_data.substr(header_end + 4);
    }
    
    return result;
}

void HttpRequestParser::parseRequestLine(const std::string& line, ParsedRequest& result) {
    std::istringstream iss(line);
    iss >> result.method >> result.path >> result.http_version;
    
    // Parse query parameters
    size_t query_pos = result.path.find('?');
    if (query_pos != std::string::npos) {
        std::string query_string = result.path.substr(query_pos + 1);
        result.path = result.path.substr(0, query_pos);
        parseQueryParams(query_string, result);
    }
}

void HttpRequestParser::parseHeader(const std::string& line, ParsedRequest& result) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        result.headers[key] = value;
    }
}

void HttpRequestParser::parseQueryParams(const std::string& query_string, ParsedRequest& result) {
    std::istringstream iss(query_string);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            // URL decode if needed
            result.query_params[key] = value;
        }
    }
}

std::string HttpRequestParser::jsonify(const ParsedRequest& req) {
    std::ostringstream json;
    
    json << "{";
    
    // Basic request fields
    json << "\"method\":\"" << req.method << "\",";
    json << "\"path\":\"" << req.path << "\",";
    json << "\"http_version\":\"" << req.http_version << "\",";
    
    // ReqID and uid
    json << "\"ReqID\":\"" << req.ReqID << "\",";
    json << "\"uid\":\"" << req.uid << "\",";
    
    // Headers
    json << "\"headers\":{";
    bool first_header = true;
    for (const auto& header : req.headers) {
        if (!first_header) {
            json << ",";
        }
        json << "\"" << header.first << "\":\"" << header.second << "\"";
        first_header = false;
    }
    json << "},";
    
    // Query parameters
    json << "\"query_params\":{";
    bool first_param = true;
    for (const auto& param : req.query_params) {
        if (!first_param) {
            json << ",";
        }
        json << "\"" << param.first << "\":\"" << param.second << "\"";
        first_param = false;
    }
    json << "},";
    
    // Body (need to handle escaping for JSON)
    json << "\"body\":";
    if (req.body.empty()) {
        json << "null";
    } else {
        // Simple escaping for JSON string
        json << "\"";
        for (char c : req.body) {
            switch (c) {
                case '"':  json << "\\\""; break;
                case '\\': json << "\\\\"; break;
                case '\b': json << "\\b"; break;
                case '\f': json << "\\f"; break;
                case '\n': json << "\\n"; break;
                case '\r': json << "\\r"; break;
                case '\t': json << "\\t"; break;
                default:
                    if (c >= 0 && c <= 0x1F) {
                        // Control characters
                        json << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else {
                        json << c;
                    }
                    break;
            }
        }
        json << "\"";
    }
    
    json << "}";
    
    return json.str();
}

} // namespace Classifier