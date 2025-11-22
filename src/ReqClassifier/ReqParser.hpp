#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include <string>
#include <unordered_map>

namespace Classifier {

class HttpRequestParser {
public:
    struct ParsedRequest {
        std::string method;
        std::string path;
        std::string http_version;
        std::unordered_map<std::string, std::string> headers;
        std::unordered_map<std::string, std::string> query_params;
        std::string body;
        std::string ReqID;
        std::string uid;

    };

    static ParsedRequest parse(const std::string& uid,const std::string& request_data);
    static std::string jsonify(const ParsedRequest& req);

private:
    static void parseRequestLine(const std::string& line, ParsedRequest& result);
    static void parseHeader(const std::string& line, ParsedRequest& result);
    static void parseQueryParams(const std::string& query_string, ParsedRequest& result);
};

} // namespace Classifier

#endif // HTTP_REQUEST_PARSER_H