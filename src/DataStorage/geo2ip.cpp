#include "geo2ip.h"
#include "../common/logger.h"


    bool geo2ip::openDB(const std::string& dbpath = geo2ip::DBPATH){
        int status = MMDB_open(dbpath.c_str(),MMDB_MODE_MMAP,&mmdb);
           if (status != MMDB_SUCCESS) {
            LOG_FATAL("Cannot open geo2ip db")
            return false;
        }
        is_open = true;
        return true;
    }
     std::string getCountryByIP(const std::string& ip) {
        if (!is_open) {
            LOG_WARN("Database is not opened")
            openDB();

        }

        int gai_error, mmdb_error;
        MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip.c_str(), 
                                                        &gai_error, &mmdb_error);

        if (gai_error != 0) {
            LOG_ERROR("DNS Error: " + std::to_string(gai_error));
        }

        if (mmdb_error != MMDB_SUCCESS) {
            LOG_ERROR("MMDB Error: " + std::to_string(mmdb_error));
        }

        if (!result.found_entry) {
            LOG_WARN ("IP not found in database");
        }

        // Получаем название страны на английском
        MMDB_entry_data_s entry_data;
        int status = MMDB_get_value(&result.entry, &entry_data, 
                                   "country", "names", "en", NULL);


        // Пробуем получить код страны
        status = MMDB_get_value(&result.entry, &entry_data, 
                               "country", "iso_code", NULL);

        if (status == MMDB_SUCCESS && entry_data.has_data) {
            LOG_INFO("Found code for client " + ip + "county is  "  + entry_data.utf8_string);
            return std::string(entry_data.utf8_string, entry_data.data_size);
        }
        LOG_WARN("Country information not available");
        return "Country information not available";
    }
