#pragma once

#include <string>
#include <vector>

namespace Argparcer {

    enum ErrorCodes {
        KEY_NOT_UNIQUE = -1,
        REQUIRE_VALUE = -2,
        REQUIRED_MISSING = -3
    };

    enum supportedTypes {
        bool_type,
        string_type,
        int_type,
        float_type
    };

    class Key {
    public:
        std::string name;
        std::string val;
        bool is_required;
        std::string helpstring;
        supportedTypes astype;

        explicit Key(std::string flag, std::string helpstring, bool required_, supportedTypes astype);
    };

    class Argparcer {
    public:
        std::vector<Key> registered_keys;
        std::vector<std::string> positional_args;

        int register_key(std::string flag, std::string helpstring, bool required, supportedTypes astype);
        static Argparcer& the();
        int parse(int argc, char** argv);
        std::string get(std::string s);

    private:
        std::string isvalid(std::string s);
    };

} // namespace Argparcer
