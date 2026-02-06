#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace Userland{

    class User{
    public:

    static std::optional<User> load_web_token(const std::string& token, int* err);

    enum Permissions : uint8_t {

    PERSONAL_DATA_VIEW = 0,
    VIEW_CLIENTS = 1,
    VIEW_AGENTS_STATS = 2
    };

    bool hasPermission(User::Permissions perm);

    // Constructor to create User from database data
    User(int user_id, int user_flags) : user_id(user_id), flags(user_flags) {}

    private:
    int user_id;
    int flags;
    User() = delete;


    };
    
    };
