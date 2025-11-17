#include <string>
#include <memory>
#include <minio/api.h>

class MinIOStorage {
public:
    MinIOStorage(const std::string& endpoint, const std::string& access_key, 
                 const std::string& secret_key, bool use_ssl = false);
    
    bool StoreRequest(const std::string& request_id, const std::string& request_data);
    std::string RetrieveRequest(const std::string& request_id);
    bool DeleteRequest(const std::string& request_id);
    
private:
    std::unique_ptr<minio::s3::BaseClient> client_;
    std::string bucket_name_ = "honeypot-requests";
};