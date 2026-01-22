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
    VIEW_CLIENTS = 1
    };

    bool hasPermission(User::Permissions perm);



    private:
    int flags;
    User() = delete;


    };
    
    };
