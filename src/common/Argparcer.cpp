#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include "Argparcer.h"

namespace Argparcer{

    class Key{

    public:
        std::string val;
        bool is_required;
        std::string helpstring;
        Argparcer::supportedTypes astype;  // исправлено: supportedTypes с маленькой буквы



        explicit Key(std::string flag, std::string helpstring, bool required_ = false, Argparcer::supportedTypes astype)
        :
        val(flag),
        helpstring(helpstring),
        is_required(required_),
        astype(astype)
        {}   
    };



    class Argparcer{
    public:
        std::vector<Argparcer::Key> registered_keys;
        std::vector<Argparcer::Key> required_keys;

    int register_key(std::string flag, std::string helpstring, bool required = false, Argparcer::supportedTypes astype){
        for(const auto& it : registered_keys ){
            if (key.val== flag){return KEY_NOT_UNIQUE;}
        }

        Argparcer::Key new_key = Key(flag,helpstring,required,astype);
        registered_keys.push_back(new_key);
        if(required){
            registered_keys.push_back(new_key);
        }
    static Argparser& the(){
        static Argparcer instance;
        return instance;

    }

};
}