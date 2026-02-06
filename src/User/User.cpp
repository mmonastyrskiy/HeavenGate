#include "../common/generic.h"
#include "User.hpp"
#include "../DataStorage/PostgressQLManager.hpp"
#include "../DataStorage/PostgressQL_query.hpp"
#include <pqxx/pqxx>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Userland{

// Helper function to compute SHA-256 hash of token using PostgreSQL's digest function
// This is more secure as it uses PostgreSQL's built-in cryptographic functions
// Note: Requires pgcrypto extension: CREATE EXTENSION IF NOT EXISTS pgcrypto;
static std::string hashToken(pqxx::connection& conn, const std::string& token) {
    try {
        // Use a separate transaction for hashing to avoid conflicts
        pqxx::nontransaction ntxn(conn);
        // Use PostgreSQL's digest function for SHA-256 hashing
        // This requires pgcrypto extension, but it's more secure
        pqxx::result result = ntxn.exec("SELECT encode(digest($1, 'sha256'), 'hex')", token);
        if (!result.empty()) {
            return result[0][0].as<std::string>();
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error hashing token (pgcrypto extension may not be installed): " + std::string(e.what()));
        LOG_ERROR("Please run: CREATE EXTENSION IF NOT EXISTS pgcrypto;");
        // Return empty string to indicate error
    }
    return "";
}

std::optional<User> User::load_web_token(const std::string& token, int* err){
    if (err) *err = 0;
    
    // Validate token format (alphanumeric only, as per existing validation)
    if (token.empty() || !std::all_of(token.begin(), token.end(), ::isalnum)) {
        if (err) *err = 1; // token is invalid
        return std::nullopt;
    }
    
    try {
        PostgressQLManager pgm;
        if (!pgm.is_connected()) {
            pgm.connect();
            if (!pgm.is_connected()) {
                if (err) *err = 3; // database connection failed
                return std::nullopt;
            }
        }
        
        auto& conn = pgm.get_connection();
        
        // Hash the token for secure storage comparison using PostgreSQL's digest function
        std::string token_hash = hashToken(conn, token);
        if (token_hash.empty()) {
            if (err) *err = 3; // database error
            return std::nullopt;
        }
        
        // Use prepared statement for security
        // Query: SELECT user_id, u.user_permissions 
        //        FROM user_web_tokens t
        //        JOIN users u ON t.user_id = u.user_id
        //        WHERE t.token_hash = $1 
        //        AND t.expires_at > NOW()
        //        AND u.is_active = TRUE
        
        pqxx::work txn(conn);
        
        // Prepare statement if not already prepared
        if (!conn.has_prepared_statement("load_web_token")) {
            conn.prepare("load_web_token",
                "SELECT t.user_id, u.user_permissions, t.expires_at "
                "FROM user_web_tokens t "
                "JOIN users u ON t.user_id = u.user_id "
                "WHERE t.token_hash = $1 "
                "AND t.expires_at > NOW() "
                "AND u.is_active = TRUE");
        }
        
        pqxx::result result = txn.exec_prepared("load_web_token", token_hash);
        
        if (result.empty()) {
            // Token not found or expired
            if (err) *err = 2; // token is expired (or invalid)
            txn.commit();
            return std::nullopt;
        }
        
        // Extract user data
        int user_id = result[0][0].as<int>();
        int user_permissions = result[0][1].as<int>();
        
        // Update last_used_at timestamp
        if (!conn.has_prepared_statement("update_token_last_used")) {
            conn.prepare("update_token_last_used",
                "UPDATE user_web_tokens "
                "SET last_used_at = NOW() "
                "WHERE token_hash = $1");
        }
        txn.exec_prepared("update_token_last_used", token_hash);
        
        txn.commit();
        
        // Create and return User object
        return User(user_id, user_permissions);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error loading web token: " + std::string(e.what()));
        if (err) *err = 3; // database error
        return std::nullopt;
    }
}

bool User::hasPermission(User::Permissions perm){
    return (flags &(1 << static_cast<uint8_t> (perm))) != 0;
}
}