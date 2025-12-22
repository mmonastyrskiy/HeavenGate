#pragma once
#include <string>
#include <vector>
#include "../common/generic.h"

enum class PSQLTables {
    INVALID,
    VISITORS_TABLE,
    USERS_TABLE,
    USERS_WEB_SESSION,

};


struct table_description {
    std::vector<std::string> cols;
    std::string create;
};

table_description clients = {{"user_id","user_ip","user_geo","unix_timestamp"},
                
                R"(
                CREATE TABLE clients (
                client_id VARCHAR(16) PRIMARY KEY,
                client_ip VARCHAR(20) NOT NULL,
                client_geo VARCHAR(10) NOT NULL,
                unix_timestamp bigint NOT_NULL DEFAULT extract(epoch from now()),
                is_active BOOLEAN DEFAULT TRUE

                );
            )"
};
table_description users = {{"user_id","invited_by","pw_hash","pw_salt","pw_update_date","user_permissions","is_active"},
R"(CREATE TABLE users (
    user_id BIGSERIAL PRIMARY KEY,
    invited_by BIGINT REFERENCES users(user_id),
    pw_hash VARCHAR(255) NOT NULL,
    pw_salt VARCHAR(50) NOT NULL,
    pw_update_date TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    user_permissions INTEGER DEFAULT 0,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    CONSTRAINT fk_invited_by FOREIGN KEY (invited_by) 
        REFERENCES users(user_id) 
        ON DELETE SET NULL
);

-- Индексы для улучшения производительности
CREATE INDEX idx_users_invited_by ON users(invited_by);
CREATE INDEX idx_users_is_active ON users(is_active);
CREATE INDEX idx_users_pw_update_date ON users(pw_update_date))"
};



PSQLTables lookup_table(const std::string& tablename){
    if(tablename == "clinets"){
        return PSQLTables::VISITORS_TABLE;
    }
    if(tablename == "users"){
        return PSQLTables::USERS_TABLE;
    }
    else{
    LOG_FATAL("Uknown table" + tablename +" lookup");
    return PSQLTables::INVALID;
    }
}
std::string get_create_statement(PSQLTables tablename)  {
    switch (tablename)
    {
    case PSQLTables::VISITORS_TABLE: 
        return clients.create;
    case PSQLTables::USERS_TABLE:
    return users.create;
    
    default:
        VERIFY_NOT_REACHED();
        return {};
    }
}

std::vector<std::string> get_cols(PSQLTables tablename)  {
    switch (tablename)
    {
    case PSQLTables::VISITORS_TABLE: 
        return clients.cols;
    
    case PSQLTables::USERS_TABLE:
    return users.cols;
    
    default:
        VERIFY_NOT_REACHED();
        return {};
    }
} 