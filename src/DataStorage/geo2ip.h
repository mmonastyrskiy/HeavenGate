#ifndef GEO2IP_H
#define GEO2IP_H
#pragma once
#include "../common/generic.h"
#include "../../thirdparty/libmaxminddb/include/maxminddb.h"
#include <string>

class geo2ip {
public:
    static constexpr const char* DEFAULT_DB_PATH = "../../assets/GeoLite2-Country.mmdb";
    
    // Constructors
    geo2ip();
    geo2ip(const std::string& db_path);
 ~geo2ip() {
    closeDB();
}
    
    // Public methods
    bool openDB(const std::string& db_path = DEFAULT_DB_PATH);
    std::string getCountryByIP(const std::string& ip);
    bool isOpen() const { return is_open; }
    void closeDB();

private:
    MMDB_s mmdb;
    bool is_open = false;
};

#endif // GEO2IP_H