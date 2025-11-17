
#include "MiniOManager.hpp"
#include <iostream>
#include <sstream>

MinIOStorage::MinIOStorage(const std::string& endpoint, const std::string& access_key,
                         const std::string& secret_key, bool use_ssl) {
    
    minio::s3::BaseClient* client;
    if (use_ssl) {
        client = new minio::s3::Client(minio::s3::Endpoint(endpoint),
                                     minio::s3::AccessKey(access_key),
                                     minio::s3::SecretKey(secret_key));
    } else {
        client = new minio::s3::Client(minio::s3::Endpoint(endpoint),
                                     minio::s3::AccessKey(access_key),
                                     minio::s3::SecretKey(secret_key));
    }
    client_.reset(client);
    
    // Создаем bucket если не существует
    try {
        bool found = client_->BucketExists(bucket_name_).get();
        if (!found) {
            client_->MakeBucket(bucket_name_).get();
        }
    } catch (const std::exception& e) {
        std::cerr << "MinIO initialization error: " << e.what() << std::endl;
    }
}

bool MinIOStorage::StoreRequest(const std::string& request_id, const std::string& request_data) {
    try {
        std::stringstream data_stream(request_data);
        minio::s3::PutObjectArgs args(
            minio::s3::Bucket(bucket_name_),
            minio::s3::Object(request_id),
            std::make_shared<std::stringstream>(data_stream),
            request_data.size(),
            minio::s3::ContentType("application/json")
        );
        
        auto response = client_->PutObject(args).get();
        return response;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to store request: " << e.what() << std::endl;
        return false;
    }
}

std::string MinIOStorage::RetrieveRequest(const std::string& request_id) {
    try {
        minio::s3::GetObjectArgs args(
            minio::s3::Bucket(bucket_name_),
            minio::s3::Object(request_id)
        );
        
        auto response = client_->GetObject(args).get();
        std::stringstream ss;
        ss << response.stream().rdbuf();
        return ss.str();
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to retrieve request: " << e.what() << std::endl;
        return "";
    }
}