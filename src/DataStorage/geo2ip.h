#include "../common/generic.h"
#include <maxminddb.h>
#include <string>



class geo2ip{
    public:
    static constexpr const char* DBPATH = "../../assets/GeoLite2-Country.mmdb";
    bool is_open {false};
    bool openDB(const std::string& dbpath);
    std::string getCountryByIP(const std::string& ip);



    private:
    MMDB_s mmdb;
    bool isopen {false};
    ~geo2ip();


};