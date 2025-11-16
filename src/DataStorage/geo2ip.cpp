#include "geo2ip.h"
#include "../common/logger.h"
#include <stdexcept>
#include <cstring>

geo2ip::geo2ip() {
    // Initialize MMDB structure
    memset(&mmdb, 0, sizeof(mmdb));
}

geo2ip::geo2ip(const std::string& db_path) {
    memset(&mmdb, 0, sizeof(mmdb));
    if (!openDB(db_path)) {
        throw std::runtime_error("Failed to open GeoIP database: " + db_path);
    }
}

bool geo2ip::openDB(const std::string& db_path) {
    if (is_open) {
        closeDB();
    }
    
    int status = MMDB_open(db_path.c_str(), MMDB_MODE_MMAP, &mmdb);
    if (status != MMDB_SUCCESS) {
        LOG_ERROR("Cannot open geo2ip database: " + db_path + ", error: " + std::to_string(status));
        return false;
    }
    
    is_open = true;
    LOG_INFO("GeoIP database opened successfully: " + db_path);
    return true;
}

std::string geo2ip::getCountryByIP(const std::string& ip) {
    if (!is_open) {
        LOG_WARN("GeoIP database is not open, attempting to open default database");
        if (!openDB()) {
            return "Database not available";
        }
    }

    int gai_error = 0, mmdb_error = 0;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip.c_str(), &gai_error, &mmdb_error);

    if (gai_error != 0) {
        LOG_ERROR("GeoIP DNS Error for IP " + ip + ": " + std::to_string(gai_error));
        return "Lookup error";
    }

    if (mmdb_error != MMDB_SUCCESS) {
        LOG_ERROR("GeoIP MMDB Error for IP " + ip + ": " + std::to_string(mmdb_error));
        return "Database error";
    }

    if (!result.found_entry) {
        LOG_DEBUG("IP not found in GeoIP database: " + ip);
        return "Unknown";
    }

    // Try to get country ISO code first
    MMDB_entry_data_s entry_data;
    int status = MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);

    if (status == MMDB_SUCCESS && entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
        std::string country_code(entry_data.utf8_string, entry_data.data_size);
        LOG_DEBUG("Found country code for IP " + ip + ": " + country_code);
        return country_code;
    }

    // Fallback to country name in English
    status = MMDB_get_value(&result.entry, &entry_data, "country", "names", "en", NULL);
    if (status == MMDB_SUCCESS && entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING) {
        std::string country_name(entry_data.utf8_string, entry_data.data_size);
        LOG_DEBUG("Found country name for IP " + ip + ": " + country_name);
        return country_name;
    }

    LOG_DEBUG("Country information not available for IP: " + ip);
    return "Unknown";
}

