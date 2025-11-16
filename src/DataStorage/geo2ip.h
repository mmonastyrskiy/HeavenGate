#include "../common/generic.h"
#include <maxminddb.h>
#include <string>

 constexpr char* DBPATH = "../../assets/GeoLite2-Country.mmdb";

class geo2ip{
    public:
    bool openDB(const std::string& dbpath);
    std::string getCountryByIP(const std::string& ip);



    private:
    MMDB_s mmdb;
    bool isopen {false};
    ~geo2ip();


};