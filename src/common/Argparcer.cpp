#include "Argparcer.h"
#include "logger.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Argparcer {

    Key::Key(std::string flag, std::string helpstring, bool required_, supportedTypes astype)
    : name(flag),
    is_required(required_),
    helpstring(helpstring),
    astype(astype)
    {}

    int Argparcer::register_key(std::string flag, std::string helpstring, bool required, supportedTypes astype) {
        for(const auto& it : registered_keys) {
            if (it.name == flag) {
                LOG_WARN( "The flag " + flag + " already registered\n"+
                "SKIPPING!");
                return KEY_NOT_UNIQUE;
            }
        }

        Key new_key = Key(flag, helpstring, required, astype);
        registered_keys.push_back(new_key);
        return 0;
    }

    Argparcer& Argparcer::the() {
        static Argparcer instance;
        return instance;
    }

    int Argparcer::parse(int argc, char** argv) {
        for(int i = 1; i < argc; i++) {
            std::string arg(argv[i]);

            if(arg.size() > 1 && arg[0] == '-') {
                std::string validation = isvalid(arg);

                if(validation == std::to_string(REQUIRE_VALUE)) {
                    if(i + 1 >= argc || std::string(argv[i + 1]).front() == '-') {
                        LOG_FATAL("Value for " + arg + "Is missing");
                        return REQUIRE_VALUE;
                    }

                    auto it = std::find_if(registered_keys.begin(), registered_keys.end(),
                                           [&](const Key& key) { return key.name == arg; });
                    if(it != registered_keys.end()) {
                        it->val = argv[i + 1];
                    }
                    i++; // Skip the value
                }
                else if(validation != "") {
                    auto it = std::find_if(registered_keys.begin(), registered_keys.end(),
                                           [&](const Key& key) { return key.name == arg; });
                    if(it != registered_keys.end()) {
                        it->val = "1";
                    }
                }
            }
            else {
                positional_args.push_back(argv[i]);
            }
        }

        for(const auto& key : registered_keys) {
            if(key.is_required && key.val.empty()) {
                LOG_FATAL("Arg" + key.name + "Is not valid please check it again");
                return REQUIRED_MISSING;
            }
        }

        return 0;
    }

    std::string Argparcer::get(std::string s) {
        for(auto &it : registered_keys) {
            if(it.name == s) {
                return it.val;
            }
        }
        LOG_WARN("Unable to get flag " + s );
        return "";
    }

    std::string Argparcer::isvalid(std::string s) {
        for(auto it : registered_keys) {
            if (it.name == s) {
                if(it.astype == supportedTypes::bool_type) {
                    return it.name;
                } else {
                    LOG_WARN("Arg" + s + "Is not valid please check it again");
                    return std::to_string(REQUIRE_VALUE);
                }
            }
        }
        return "";
    }

} // namespace Argparcer
