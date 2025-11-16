#include "../common/generic.h"
#include <maxminddb.h>
#include <string>

class geo2ip{
    public:
    constexpr std::string DBPATH = "../../assets/GeoLite2-Country.mmdb";
    bool openDB(const std::string& dbpath);
    std::string getCountryByIP(const std::string& ip);



    private:
    MMDB_s mmdb;
    bool isopen {false};
    ~geo2ip();


};