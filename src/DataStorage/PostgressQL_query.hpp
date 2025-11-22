#pragma once
#include <string>
#include <vector>
#include "../common/generic.h"

enum class PSQLTables {
    VISITORS_TABLE
};


typedef struct table_description {
    std::vector<std::string> cols;
    std::string create;
};

table_description clients = {{"user_id","user_ip","user_geo","unix_timestamp"},
                
                R"(
                CREATE TABLE clients (
                user_id VARCHAR(16) PRIMARY KEY,
                user_ip VARCHAR(20) NOT NULL,
                user_geo VARCHAR(10) NOT NULL,
                unix_timestamp bigint NOT_NULL DEFAULT extract(epoch from now()),
                is_active BOOLEAN DEFAULT TRUE

                );
            )"
};



PSQLTables lookup_table(const std::string& tablename){
    if(tablename == "clinets"){
        return PSQLTables::VISITORS_TABLE;
    }
    else{
    LOG_FATAL("Uknown table" + tablename +" lookup");
    }
}
std::string get_create_statement(PSQLTables tablename)  {
    switch (tablename)
    {
    case PSQLTables::VISITORS_TABLE: 
        return clients.create;
    
    default:
        VERIFY_NOT_REACHED();
    }
}

std::vector<std::string> get_cols(PSQLTables tablename)  {
    switch (tablename)
    {
    case PSQLTables::VISITORS_TABLE: 
        return clients.cols;
    
    default:
        VERIFY_NOT_REACHED();
    }
} 