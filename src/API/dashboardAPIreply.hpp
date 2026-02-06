#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>

std::unordered_map<int,std::string> emap = {
    {1, "token is invalid"},
    {2, "token is expired"},
    {3, "database error"}
};

// Helper function to escape JSON string values
inline static std::string escapeJsonString(const std::string& str) {
    std::ostringstream o;
    for (size_t i = 0; i < str.length(); ++i) {
        switch (str[i]) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= str[i] && str[i] <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)str[i];
                } else {
                    o << str[i];
                }
        }
    }
    return o.str();
}

inline static std::string ret_401(const int& e ){
    // Safe access to error map with default fallback
    std::string errorMsg = "Unknown error";
    auto it = emap.find(e);
    if (it != emap.end()) {
        errorMsg = it->second;
    }
    
    std::string escapedData = escapeJsonString(errorMsg);
    std::string reply = "{"
                          "\"code\":\"401\","
                          "\"success\":\"false\","
                          "\"data\":\"" + escapedData + "\""
                          "}";
    return reply;
}

inline static std::string ret_403(const std::string& s ){
    std::string escapedData = escapeJsonString(s);
    std::string reply = "{"
                          "\"code\":\"403\","
                          "\"success\":\"false\","
                          "\"data\":\"" + escapedData + "\""
                          "}";
    return reply;
}


inline static std::string ret_500(const std::string& s ){
    std::string escapedData = escapeJsonString(s);
    std::string reply = "{"
                          "\"code\":\"500\","
                          "\"success\":\"false\","
                          "\"data\":\"" + escapedData + "\""
                          "}";
    return reply;
}

inline static std::string ret_200(const std::string& s ){
    std::string reply = "{"
                          "\"code\":\"200\","
                          "\"success\":\"true\","
                          "\"data\":" + s
                          "}";
    return reply;
}