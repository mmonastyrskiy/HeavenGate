#include <unordered_map>
#include <string>
#include <vector>





namespace Argparcer{

#define KEY_NOT_UNIQUE -1

    enum class supportedTypes {
        int_type,
        string_type,
        bool_type
    };  // исправлено: добавлена точка с запятой

    class Key{
        public:
        // Исправлено: supportedTypes с маленькой буквы и инициализация полей
        Key(std::string flag, std::string helpstring, bool required = false, Argparcer::supportedTypes astype);
        ~Key() = default;
    };

    class Argparcer {
    
    static Argparcer& the();

        // Исправлено: supportedTypes с маленькой буквы
        int register_key(std::string flag, std::string helpstring, bool required = false, Argparcer::supportedTypes astype);
        void parse(int argc, char** argv);

    private:    
        Argparcer() = default;
        ~Argparcer() = default;
        void create_auto_help_page();
    };
}