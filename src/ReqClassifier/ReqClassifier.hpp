#include <string>
#include "ReqParser.hpp"
#include "../DataStorage/ElasticStorage.hpp"
#include "../common/Confparcer.h"
#include "../common/logger.h"
namespace Classifier{
    class ReqClassifier //TODO: MOVE IMPLEMENTATIONS TO A STANALLONE CPP FILE
    {
    private:
        /* data */
    public:
        ReqClassifier() = delete;
        ~ReqClassifier() = delete;
    };
    static void ProcessReq(const std::string& uid,const std::string& request_data){
        auto req = Classifier::HttpRequestParser::parse(uid,request_data);
        auto json = Classifier::HttpRequestParser::jsonify(req);

        auto ehost = Confparcer::SETTING<std::string>("ELASTIC_HOST","127.0.0.1");
        auto eport = Confparcer::SETTING<size_t>("ELASTIC_HOST",9200);
        SimpleElasticsearchClient elastic(ehost,static_cast<int> (eport)); // FIXME: MAY CAUSE PROBLEMS DUE TO INT LIMITS EXCEED
        if(!elastic.testConnection()){
            LOG_ERROR("Cannot verify connection to the Elastic");
            return;
    }
        if(!elastic.indexDocument(REQ_INDEX,json)){
            LOG_ERROR("Failed to index request in Elastic");
    }
    LOG_INFO("Request Indexed Successfully");
    }
    
}