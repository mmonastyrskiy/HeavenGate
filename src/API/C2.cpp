#include <crow.h>
#include "DashboardAPI.h"  // Предполагается, что DashboardAPI объявлен здесь

int main() {
    crow::SimpleApp app;
    
    // Маршрут для получения клиентов
    CROW_ROUTE(app, "/api/clients/get")
    .methods("GET"_method)([](const crow::request& req) {
        // Извлекаем токен из заголовков
        std::string token = req.get_header_value("Authorization");
        
        // Очищаем от префикса "Bearer " если есть
        if (token.find("Bearer ") == 0) {
            token = token.substr(7);
        }
        
        if (token.empty()) {
            return crow::response(401, "Missing authorization token");
        }
        
        DashboardAPI api;
        std::string result = api.callGetClients(token);
        
        // Парсим результат из вашей функции (предполагается формат JSON)
        // Ваша функция возвращает разные статусы через ret_XXX функции
        // Нужно преобразовать их в crow::response
        
        // Простейший парсинг (можно улучшить в зависимости от вашей реализации)
        if (result.find("\"status\":401") != std::string::npos) {
            return crow::response(401, result);
        } else if (result.find("\"status\":403") != std::string::npos) {
            return crow::response(403, result);
        } else if (result.find("\"status\":500") != std::string::npos) {
            return crow::response(500, result);
        } else if (result.find("\"status\":200") != std::string::npos) {
            return crow::response(200, result);
        }
        
        return crow::response(200, result);
    });
    
    // Дополнительно: обработка CORS (если нужно)
    app.loglevel(crow::LogLevel::Warning);
    
    // Запуск сервера на порту 8080
    app.port(8000).multithreaded().run();
    
    return 0;
}