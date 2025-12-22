#include "../common/generic.h"
#include "User.hpp"
namespace Userland{

std::optional<User> load_web_token(const std::string token, int* err){
    TODO();
}

bool User::hasPermission(User::Permissions perm){
    return (flags &(1 << static_cast<uint8_t> (perm))) != 0;
}
}