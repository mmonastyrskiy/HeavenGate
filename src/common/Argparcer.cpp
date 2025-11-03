#include "Argparcer.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Argparcer{

    class Key{

    public:
        std::string name;
        std::string val;
        bool is_required;
        std::string helpstring;
        supportedTypes astype;




        explicit Key(std::string flag, std::string helpstring, bool required_ = false, supportedTypes astype)
        :
        name(flag),
        helpstring(helpstring),
        is_required(required_),
        astype(astype)
        {}   
    };



    class Argparcer{
    public:
        std::vector<Key> registered_keys;
        std::vector<Key> required_keys;
        std::vector<std::string> positional_args;


    int register_key(std::string flag, std::string helpstring, bool required = false, supportedTypes astype){
        for(const auto& it : registered_keys ){
            if (it.name== flag){
                std::cout << "The flag" << flag <<"Already registered"<< std::endl;
                
                return KEY_NOT_UNIQUE;
            }
        }

        Key new_key = Key(flag,helpstring,required,astype);
        registered_keys.push_back(new_key);
        if(required){
            registered_keys.push_back(new_key);
        }
        return 0;



}
    static Argparcer& the(){
        static Argparcer instance;
        return instance;

    }
   


    int parse(char** argv){
    for(int i = 1; argv[i] != nullptr; i++) {
    std::string arg(argv[i]);
    
    // Если это флаг
    if(arg.size() > 1 && arg[0] == '-') {
        std::string validation = isvalid(arg);
        
        if(validation == std::to_string(REQUIRE_VALUE)) {
            // Флаг требует значение
            if(argv[i+1] == nullptr || std::string(argv[i+1]).front() == '-') {
                std::cout << "Value for " << arg << " is missing" << std::endl;
                return REQUIRE_VALUE;
            }
            
            // Находим и устанавливаем значение
            auto it = std::find_if(registered_keys.begin(), registered_keys.end(),
                [&](const Key& key) { return key.name == arg; });
            if(it != registered_keys.end()) {
                it->val = argv[i+1];
            }
            i++; // Пропускаем значение
        } 
        else if(validation != "") {
            // Флаг без значения
            auto it = std::find_if(registered_keys.begin(), registered_keys.end(),
                [&](const Key& key) { return key.name == arg; });
            if(it != registered_keys.end()) {
                it->val = "1";
            }
        }
    } 
    else {
        // Позиционный аргумент (не флаг)
        positional_args.push_back(argv[i]);
    }
}
    }

private:
 std::string isvalid(std::string s ){
        for(auto it : registered_keys){
            if (it.name == s){
                if(it.astype == supportedTypes::bool_type){return it.name;}
                else{return std::to_string(REQUIRE_VALUE);}
                
            }

        }
        return "";
    }
};
}