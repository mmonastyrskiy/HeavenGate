#include <string>
#include <unordered_map>

std::unordered_map<int,std::string> emap = {
    {1, "token is invalid"},
    {2, "token is expired"}
};

inline static std::string ret_401(const int& e ){
    std::string reply = "{"
                          "\"code\":\"401"  "\","
                          "\"success\":\"false" "\","
                          "\"data\":" +emap[e] + "\""
                          "}";
}

inline static std::string ret_403(const std::string s ){
    std::string reply = "{"
                          "\"code\":\"403"  "\","
                          "\"success\":\"false" "\","
                          "\"data\":" +s + "\""
                          "}";
}


inline static std::string ret_500(const std::string s ){
    std::string reply = "{"
                          "\"code\":\"500"  "\","
                          "\"success\":\"false" "\","
                          "\"data\":" +s + "\""
                          "}";
}

inline static std::string ret_200(const std::string s ){
    std::string reply = "{"
                          "\"code\":\"200"  "\","
                          "\"success\":\"true" "\","
                          "\"data\":" +s + "\""
                          "}";
}