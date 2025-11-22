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

table_description visitors = {{"emp_id","first_name","last_name","department","salary","hire_date"},
                
                R"(
                CREATE TABLE employees (
                    emp_id SERIAL PRIMARY KEY,
                    first_name VARCHAR(50) NOT NULL,
                    last_name VARCHAR(50) NOT NULL,
                    department VARCHAR(50),
                    salary DECIMAL(10,2),
                    hire_date DATE DEFAULT CURRENT_DATE
                )
            )"
};



PSQLTables lookup_table(const std::string& tablename){
    if(tablename == "visitors"){
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
        return visitors.create;
    
    default:
        VERIFY_NOT_REACHED();
    }
}

std::vector<std::string> get_cols(PSQLTables tablename)  {
    switch (tablename)
    {
    case PSQLTables::VISITORS_TABLE: 
        return visitors.cols;
    
    default:
        VERIFY_NOT_REACHED();
    }
} 